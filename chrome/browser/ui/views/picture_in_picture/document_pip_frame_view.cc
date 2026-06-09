// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/picture_in_picture/document_pip_frame_view.h"

#include <memory>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_occlusion_tracker.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_specification.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"
#include "chrome/browser/ui/views/picture_in_picture/document_pip_host.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/location_bar_model_util.h"
#include "components/security_state/content/security_state_tab_helper.h"
#include "components/security_state/core/security_state.h"
#include "components/url_formatter/elide_url.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/event_observer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/event_monitor.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/window_shape.h"
#include "url/gurl.h"

namespace {

// These constants mirror PictureInPictureBrowserFrameView, which this class
// replaces for the standalone document PiP path.
constexpr int kButtonIconSize = 16;
constexpr int kTopControlsHeight = 34;
constexpr int kResizeBorder = 10;
constexpr int kResizeAreaCornerSize = 16;

// Match PictureInPictureBrowserFrameView's 16px top-bar icon sizes so the
// security/page-info icon aligns with the PiP window controls.
constexpr int kSecurityIconImageSize = 16;

// Creates an ImageButton styled for the PiP title bar.
std::unique_ptr<views::ImageButton> CreatePipTitleBarButton(
    const gfx::VectorIcon& icon,
    const std::u16string& tooltip,
    views::Button::PressedCallback callback) {
  auto button = std::make_unique<views::ImageButton>(std::move(callback));
  button->SetImageModel(views::Button::STATE_NORMAL,
                        ui::ImageModel::FromVectorIcon(
                            icon, kColorPipWindowForeground, kButtonIconSize));
  button->SetTooltipText(tooltip);
  button->GetViewAccessibility().SetName(tooltip);
  return button;
}

// A thin clickable container that holds the security icon and origin label and
// opens the Page Info dialog when pressed. This intentionally adds no behavior
// beyond views::Button; the subclass exists only because views::Button's
// constructor is protected.
class OriginChipButton : public views::Button {
  METADATA_HEADER(OriginChipButton, views::Button)

 public:
  explicit OriginChipButton(PressedCallback callback)
      : views::Button(std::move(callback)) {}
  OriginChipButton(const OriginChipButton&) = delete;
  OriginChipButton& operator=(const OriginChipButton&) = delete;
  ~OriginChipButton() override = default;
};

BEGIN_METADATA(OriginChipButton)
END_METADATA

}  // namespace

class DocumentPipFrameView::WindowEventObserver : public ui::EventObserver {
 public:
  explicit WindowEventObserver(DocumentPipFrameView* frame_view)
      : frame_view_(frame_view) {
    event_monitor_ = views::EventMonitor::CreateWindowMonitor(
        this, frame_view_->GetWidget()->GetNativeWindow(),
        {ui::EventType::kMouseMoved, ui::EventType::kMouseExited,
         ui::EventType::kKeyPressed, ui::EventType::kKeyReleased});
  }

  WindowEventObserver(const WindowEventObserver&) = delete;
  WindowEventObserver& operator=(const WindowEventObserver&) = delete;
  ~WindowEventObserver() override = default;

  void OnEvent(const ui::Event& event) override {
    if (event.IsKeyEvent()) {
      frame_view_->UpdateTopBarView(/*render_active=*/true);
      return;
    }

    frame_view_->OnMouseEnteredOrExitedWindow(IsMouseInBounds());
  }

 private:
  bool IsMouseInBounds() {
    gfx::Point point = event_monitor_->GetLastMouseLocation();
    views::View::ConvertPointFromScreen(frame_view_, &point);
    return frame_view_->GetLocalBounds().Contains(point);
  }

  raw_ptr<DocumentPipFrameView> frame_view_;
  std::unique_ptr<views::EventMonitor> event_monitor_;
};

DocumentPipFrameView::DocumentPipFrameView(DocumentPipHost* host)
    : host_(CHECK_DEREF(host)) {
  views::Widget* const widget = host_->GetWidget();
  CHECK(widget);
  const bool disallow_return_to_opener =
      host_->GetPipOptions().disallow_return_to_opener;
  // Create the top bar container.
  AddChildView(views::Builder<views::FlexLayoutView>()
                   .CopyAddressTo(&top_bar_container_view_)
                   .SetOrientation(views::LayoutOrientation::kHorizontal)
                   .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
                   .Build());

  top_bar_container_view_->SetBackground(
      views::CreateSolidBackground(kColorPipWindowTopBarBackground));

  // Create the origin chip: a clickable button containing the security (lock)
  // icon and the opener origin label. Clicking it opens the Page Info dialog.
  // Unlike the omnibox stack, this is a thin PiP-specific control that reads
  // directly from the opener WebContents.
  auto origin_chip = std::make_unique<OriginChipButton>(base::BindRepeating(
      [](DocumentPipFrameView* frame_view) { frame_view->ShowPageInfo(); },
      // Safety: The widget owns the frame view and this button, so the
      // callback cannot outlive the widget.
      base::Unretained(this)));
  origin_chip->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_TOOLTIP_LOCATION_ICON));
  origin_chip->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_TOOLTIP_LOCATION_ICON));
  auto* origin_layout =
      origin_chip->SetLayoutManager(std::make_unique<views::FlexLayout>());
  origin_layout->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter);
  // The chip sizes to its content (lock + origin text) and shrinks (eliding the
  // origin) when space is tight, but never grows to fill the bar, so the
  // remaining top-bar area stays draggable.
  origin_chip->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kPreferred));

  // The security (lock) icon, left of the origin text.
  auto security_icon = std::make_unique<views::ImageView>();
  // The 5px vertical margin mirrors PictureInPictureBrowserFrameView's
  // IconLabelBubbleView margin. The horizontal margins define this standalone
  // chip's inset: 8px from the PiP window edge and 4px between the icon and
  // origin text.
  security_icon->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(5, 8, 5, 4));
  security_icon_ = origin_chip->AddChildView(std::move(security_icon));

  // The origin label, showing the opener's security-display host.
  auto origin_label = std::make_unique<views::Label>(
      std::u16string(), views::style::CONTEXT_LABEL,
      views::style::STYLE_PRIMARY);
  origin_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  origin_label->SetElideBehavior(gfx::ELIDE_HEAD);
  origin_label->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kPreferred));
  origin_label->SetBackgroundColor(kColorPipWindowTopBarBackground);
  origin_label->SetEnabledColor(kColorPipWindowForeground);
  origin_label_ = origin_chip->AddChildView(std::move(origin_label));

  origin_chip_ = top_bar_container_view_->AddChildView(std::move(origin_chip));

  // A flexible, non-interactive spacer that pushes the buttons to the right and
  // provides a draggable caption region between the origin chip and the
  // buttons.
  auto* spacer =
      top_bar_container_view_->AddChildView(std::make_unique<views::View>());
  spacer->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded));

  // Create a container for the top right buttons.
  button_container_view_ = top_bar_container_view_->AddChildView(
      std::make_unique<views::FlexLayoutView>());

  // Create the back-to-tab button if allowed.

  if (!disallow_return_to_opener) {
    const auto back_to_tab_cb = [](DocumentPipFrameView* frame_view) {
      frame_view->set_close_reason(
          DocumentPipFrameView::CloseReason::kBackToTabButton);
      if (!PictureInPictureWindowManager::GetInstance()
               ->ExitPictureInPictureViaWindowUi(
                   PictureInPictureWindowManager::UiBehavior::
                       kCloseWindowAndFocusOpener)) {
        frame_view->GetWidget()->Close();
      }
    };
    const auto& back_icon = features::IsRoundedIconsEnabled()
                                ? vector_icons::kBackToTabIcon
                                : vector_icons::kBackToTabChromeRefreshOldIcon;
    back_to_tab_button_ =
        button_container_view_->AddChildView(CreatePipTitleBarButton(
            back_icon,
            l10n_util::GetStringUTF16(
                IDS_PICTURE_IN_PICTURE_BACK_TO_TAB_CONTROL_TEXT),
            // Safety: The widget owns the frame view and its child buttons,
            // so the callback cannot outlive the widget.
            base::BindRepeating(back_to_tab_cb, base::Unretained(this))));
  }

  // Create the close button.
  const auto close_cb = [](DocumentPipFrameView* frame_view) {
    frame_view->set_close_reason(
        DocumentPipFrameView::CloseReason::kCloseButton);
    if (!PictureInPictureWindowManager::GetInstance()
             ->ExitPictureInPictureViaWindowUi(
                 PictureInPictureWindowManager::UiBehavior::kCloseWindowOnly)) {
      frame_view->GetWidget()->Close();
    }
  };
  const auto& close_icon = features::IsRoundedIconsEnabled()
                               ? vector_icons::kCloseIcon
                               : vector_icons::kCloseChromeRefreshOldIcon;
  close_image_button_ =
      button_container_view_->AddChildView(CreatePipTitleBarButton(
          close_icon,
          l10n_util::GetStringUTF16(IDS_PICTURE_IN_PICTURE_CLOSE_CONTROL_TEXT),
          // Safety: The widget owns the frame view and its child buttons,
          // so the callback cannot outlive the widget.
          base::BindRepeating(close_cb, base::Unretained(this))));

  // TODO(crbug.com/40279642): Don't force dark mode once we support a
  // light mode window.
  widget->SetColorModeOverride(ui::ColorProviderKey::ColorMode::kDark);

  // Observe the widget for activation changes so the top bar can render its
  // active/inactive state. Narrowed to activation + destruction; window policy
  // (visibility, bounds, tucking) is owned by DocumentPipHost.
  widget_observation_.Observe(widget);

  UpdateOriginAndSecurity();
}

DocumentPipFrameView::~DocumentPipFrameView() {
  base::UmaHistogramEnumeration("Media.DocumentPictureInPicture.CloseReason",
                                close_reason_);
}

gfx::Rect DocumentPipFrameView::GetBoundsForClientView() const {
  const auto border_thickness = FrameBorderInsets();
  const int top_height = GetTopAreaHeight();
  return gfx::Rect(border_thickness.left(), top_height,
                   width() - border_thickness.width(),
                   height() - top_height - border_thickness.bottom());
}

gfx::Rect DocumentPipFrameView::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  const auto border_thickness = FrameBorderInsets();
  const int top_height = GetTopAreaHeight();
  return gfx::Rect(
      client_bounds.x() - border_thickness.left(),
      client_bounds.y() - top_height,
      client_bounds.width() + border_thickness.width(),
      client_bounds.height() + top_height + border_thickness.bottom());
}

int DocumentPipFrameView::NonClientHitTest(const gfx::Point& point) {
  // Allow interacting with the origin chip (opens Page Info).
  if (GetOriginChipBounds().Contains(point)) {
    return HTCLIENT;
  }

  // Allow interacting with the buttons. Button bounds are interpreted in their
  // parent's coordinate space and converted to frame-view coordinates so the
  // hit-test comparisons share a coordinate space.
  if (back_to_tab_button_ &&
      ConvertControlBoundsToFrame(back_to_tab_button_).Contains(point)) {
    return HTCLIENT;
  }
  if (ConvertControlBoundsToFrame(close_image_button_).Contains(point)) {
    return HTCLIENT;
  }

  // Allow dragging and resizing the window.
  int window_component = GetHTComponentForFrame(
      point, ResizeBorderInsets(), kResizeAreaCornerSize, kResizeAreaCornerSize,
      GetWidget()->widget_delegate()->CanResize());
  if (window_component != HTNOWHERE) {
    return window_component;
  }

  // Allow interacting with the web contents.
  if (GetWidget()->client_view()) {
    int frame_component = GetWidget()->client_view()->NonClientHitTest(point);
    if (frame_component != HTNOWHERE) {
      return frame_component;
    }
  }

  return HTCAPTION;
}

void DocumentPipFrameView::GetWindowMask(const gfx::Size& size,
                                         SkPath* window_mask) {
  CHECK(window_mask);
  views::GetDefaultWindowMask(size, window_mask);
}

gfx::Size DocumentPipFrameView::GetMinimumSize() const {
  return PictureInPictureWindowManager::GetMinimumInnerWindowSize() +
         GetNonClientViewAreaSize();
}

gfx::Size DocumentPipFrameView::GetMaximumSize() const {
  if (!GetWidget() || !GetWidget()->GetNativeWindow()) {
    return GetMinimumSize();
  }

  auto display = display::Screen::Get()->GetDisplayNearestWindow(
      GetWidget()->GetNativeWindow());
  return PictureInPictureWindowManager::GetMaximumWindowSize(display);
}

void DocumentPipFrameView::Layout(PassKey) {
  gfx::Rect content_area = GetLocalBounds();
  content_area.Inset(FrameBorderInsets());
  gfx::Rect top_bar = content_area;
  top_bar.set_height(kTopControlsHeight);
  top_bar_container_view_->SetBoundsRect(top_bar);

  LayoutSuperclass<views::FrameView>(this);
}

void DocumentPipFrameView::AddedToWidget() {
  views::FrameView::AddedToWidget();
  // Create the EventMonitor here (not in the ctor): it binds to the native
  // window, which is only guaranteed to exist once the view is attached to the
  // widget. Teardown is intentionally handled in OnWidgetDestroying rather
  // than in a symmetric RemovedFromWidget; see the comment there.
  window_event_observer_ = std::make_unique<WindowEventObserver>(this);
}

int DocumentPipFrameView::GetTopAreaHeight() const {
  return FrameBorderInsets().top() + kTopControlsHeight;
}

gfx::Insets DocumentPipFrameView::FrameBorderInsets() const {
  return gfx::Insets();
}

gfx::Insets DocumentPipFrameView::ResizeBorderInsets() const {
  return gfx::Insets(kResizeBorder);
}

gfx::Size DocumentPipFrameView::GetNonClientViewAreaSize() const {
  const auto border_thickness = FrameBorderInsets();
  const int top_height = GetTopAreaHeight();
  return gfx::Size(border_thickness.width(),
                   top_height + border_thickness.bottom());
}

void DocumentPipFrameView::OnThemeChanged() {
  UpdateOriginAndSecurity();
  views::FrameView::OnThemeChanged();
}

///////////////////////////////////////////////////////////////////////////////
// views::WidgetObserver implementations:

void DocumentPipFrameView::OnWidgetActivationChanged(views::Widget* widget,
                                                     bool active) {
  UpdateTopBarView(active || mouse_inside_window_);
}

void DocumentPipFrameView::OnWidgetDestroying(views::Widget* widget) {
  // This is the only correct hook for teardown of these two members under
  // CLIENT_OWNS_WIDGET. The Widget destruction sequence is:
  //
  //   1. ~Widget runs.
  //   2. HandleWidgetDestroying() -> OnWidgetDestroying fires here.
  //   3. native_widget_->Close() destroys the native window.
  //   4. DestroyRootView() tears down the view tree, which would eventually
  //      fire RemovedFromWidget on this view.
  //
  // `window_event_observer_` owns a views::EventMonitor that holds the native
  // window, so it MUST be released before step 3. RemovedFromWidget would be
  // too late (and is dead code for this view anyway, since this frame view is
  // owned 1:1 by its widget and never reparented).
  //
  // `widget_observation_` is a ScopedObservation; its destructor calls
  // widget->RemoveObserver(this), so it must be reset before the Widget goes
  // away to avoid a use-after-free at ~DocumentPipFrameView.
  window_event_observer_.reset();
  widget_observation_.Reset();
}

gfx::Rect DocumentPipFrameView::ConvertControlBoundsToFrame(
    views::View* control_view) const {
  gfx::RectF bounds(control_view->GetMirroredBounds());
  views::View::ConvertRectToTarget(control_view->parent(), this, &bounds);
  return gfx::ToEnclosingRect(bounds);
}

gfx::Rect DocumentPipFrameView::GetOriginChipBounds() const {
  CHECK(origin_chip_);
  return ConvertControlBoundsToFrame(origin_chip_);
}

void DocumentPipFrameView::UpdateOriginAndSecurity() {
  content::WebContents* const opener_web_contents =
      host_->GetOpenerWebContents();
  const GURL url = opener_web_contents->GetLastCommittedURL();

  // Show the opener origin in security-display form (scheme omitted for the
  // common HTTPS case).
  const std::u16string origin_text = url_formatter::FormatUrlForSecurityDisplay(
      url, url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
  origin_label_->SetText(origin_text);
  origin_chip_->GetViewAccessibility().SetDescription(origin_text);

  // Derive the security level and lock/security icon directly from the opener,
  // without a LocationBarModel.
  auto* helper = SecurityStateTabHelper::FromWebContents(opener_web_contents);
  // The opener always has a SecurityStateTabHelper attached (it is created by
  // TabHelpers), so it is safe to read the security state directly.
  CHECK(helper);
  const security_state::SecurityLevel security_level =
      helper->GetSecurityLevel();
  const std::unique_ptr<security_state::VisibleSecurityState>
      visible_security_state = helper->GetVisibleSecurityState();
  const ui::ColorId foreground_color_id =
      render_active_ ? kColorPipWindowForeground
                     : kColorPipWindowForegroundInactive;
  security_icon_->SetImage(ui::ImageModel::FromVectorIcon(
      location_bar_model::GetSecurityVectorIcon(security_level,
                                                visible_security_state.get()),
      foreground_color_id, kSecurityIconImageSize));
}

void DocumentPipFrameView::UpdateTopBarView(bool render_active) {
  if (render_active_ == render_active) {
    return;
  }
  render_active_ = render_active;

  const auto* color_provider = GetColorProvider();
  const SkColor foreground = color_provider->GetColor(
      render_active_ ? kColorPipWindowForeground
                     : kColorPipWindowForegroundInactive);
  origin_label_->SetEnabledColor(foreground);
  UpdateOriginAndSecurity();
}

void DocumentPipFrameView::OnMouseEnteredOrExitedWindow(bool entered) {
  mouse_inside_window_ = entered;
  UpdateTopBarView(mouse_inside_window_ ||
                   (GetWidget() && GetWidget()->IsActive()));
}

bool DocumentPipFrameView::ShowPageInfo() {
  CHECK(origin_chip_);

  // Anchor Page Info on the PiP window's own origin chip so the dialog appears
  // over the standalone Document PiP window rather than the opener browser
  // window. PageInfoBubbleView and PageInfoBubbleSpecification live in the
  // monolith-free //chrome/browser/ui/views/page_info target, so this does not
  // reintroduce the pip -> ui -> pip include cycle.
  content::WebContents* const opener_web_contents =
      host_->GetOpenerWebContents();
  std::unique_ptr<PageInfoBubbleSpecification> specification =
      PageInfoBubbleSpecification::Builder(
          views::BubbleAnchor(origin_chip_), GetWidget()->GetNativeWindow(),
          opener_web_contents, opener_web_contents->GetLastCommittedURL())
          .HideExtendedSiteInfo()
          .Build();

  views::BubbleDialogDelegateView* const bubble =
      PageInfoBubbleView::CreatePageInfoBubble(std::move(specification));
  bubble->GetWidget()->Show();

  // Keep the Page Info bubble visible (not treated as an occluder) while it is
  // anchored to the always-on-top PiP window.
  if (PictureInPictureOcclusionTracker* tracker =
          PictureInPictureWindowManager::GetInstance()->GetOcclusionTracker()) {
    tracker->OnPictureInPictureWidgetOpened(bubble->GetWidget());
  }
  return true;
}

BEGIN_METADATA(DocumentPipFrameView)
END_METADATA
