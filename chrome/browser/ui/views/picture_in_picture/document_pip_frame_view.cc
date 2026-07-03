// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/picture_in_picture/document_pip_frame_view.h"

#include <memory>
#include <optional>
#include <string_view>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/picture_in_picture/auto_pip_setting_overlay_view.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_occlusion_tracker.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/content_settings/content_setting_image_model.h"
#include "chrome/browser/ui/toolbar/chrome_location_bar_model_delegate.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/location_bar/content_setting_image_view.h"
#include "chrome/browser/ui/views/location_bar/location_icon_state_helper.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_specification.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"
#include "chrome/browser/ui/views/picture_in_picture/document_pip_host.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/location_bar_model_impl.h"
#include "components/security_state/core/security_state.h"
#include "components/vector_icons/vector_icons.h"
#include "components/webapps/isolated_web_apps/scheme.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_constants.h"
#include "extensions/buildflags/buildflags.h"
#include "media/base/media_switches.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/event_observer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/event_monitor.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/window_shape.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/constants.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

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

// Size of the camera/microphone content-setting icons, matching
// PictureInPictureBrowserFrameView.
constexpr int kContentSettingIconSize = 16;

// Vertical margin around the content-setting icons, matching
// PictureInPictureBrowserFrameView.
constexpr int kIconViewVerticalMargin = 5;

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
  // Match PictureInPictureBrowserFrameView's OverlayWindowImageButton styling:
  // center the icon and add the standard 4px vector-image-button border insets
  // so the control is 24x24 (16px icon + 4px on each side), keeping the icon
  // 4px from the window edge and the hover highlight circular.
  button->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  button->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
  views::ConfigureVectorImageButton(button.get());
  views::InstallCircleHighlightPathGenerator(button.get());
  button->SetInstallFocusRingOnFocus(true);
  return button;
}

// The standalone Document PiP origin chip. Reuses views::IconLabelBubbleView
// (the omnibox security-chip base) for pixel-accurate icon/label geometry,
// hover/press ink-drop, the security-text slide animation, and theme handling.
// The chip's URL/security text is supplied by the frame view from a
// LocationBarModelImpl backed by the opener WebContents. Pressing the chip
// opens the Page Info dialog.
class OriginChipView : public IconLabelBubbleView {
  METADATA_HEADER(OriginChipView, IconLabelBubbleView)

 public:
  OriginChipView(const gfx::FontList& font_list,
                 IconLabelBubbleView::Delegate* delegate,
                 base::RepeatingCallback<bool()> show_page_info)
      : IconLabelBubbleView(font_list, delegate),
        show_page_info_(std::move(show_page_info)) {
    // Enable the security-text slide animation and let the explicit PiP
    // foreground color drive the label color (no auto-contrast adjustment).
    SetUpForAnimation();
    label()->SetAutoColorReadabilityEnabled(false);
  }
  OriginChipView(const OriginChipView&) = delete;
  OriginChipView& operator=(const OriginChipView&) = delete;
  ~OriginChipView() override = default;

  // Sets the security (lock) icon. Exposes the protected SetImageModel().
  void SetSecurityImage(const ui::ImageModel& image) { SetImageModel(image); }

  // Sets the security chip text, sliding it in/out when `animate` is true and
  // updating it instantly otherwise (e.g. on the first update or a theme
  // change). Mirrors LocationIconView::UpdateTextVisibility(), but takes the
  // already-computed text and animate flag (the browser-backed view pulls them
  // from a LocationBarModel) and folds in the trailing UpdateLabelColors() that
  // LocationIconView::Update() runs separately.
  void UpdateSecurityText(std::u16string_view text, bool animate) {
    SetLabel(text);
    const bool should_show = !text.empty();
    if (!animate) {
      ResetSlideAnimation(should_show);
    } else if (should_show) {
      AnimateIn(std::nullopt);
    } else {
      AnimateOut();
    }
    // UpdateLabelColors() resolves colors through the ColorProvider, which is
    // only available once this view is attached to a Widget. During the frame
    // view's construction the chip is not yet in the widget, so skip the color
    // update; OnThemeChanged() re-runs the full update after attach.
    if (GetWidget()) {
      UpdateLabelColors();
    }
  }

 protected:
  // IconLabelBubbleView:
  bool ShowBubble(const ui::Event& event) override {
    return show_page_info_.Run();
  }
  bool IsBubbleShowing() const override {
    return PageInfoBubbleView::GetShownBubbleType() !=
           PageInfoBubbleView::BUBBLE_NONE;
  }
  // Match the browser-backed LocationIconView: never draw the trailing
  // separator (there is no omnibox decoration to separate from).
  bool ShouldShowSeparator() const override { return false; }

 private:
  base::RepeatingCallback<bool()> show_page_info_;
};

BEGIN_METADATA(OriginChipView)
END_METADATA

}  // namespace

// Minimal LocationBarModelDelegate that sources the "active" WebContents from
// the standalone PiP opener. This lets the frame view drive a
// LocationBarModelImpl (and thus reuse the omnibox's URL-formatting and
// security-text logic) without the omnibox/Browser stack.
class DocumentPipFrameView::LocationBarModelDelegateImpl
    : public ChromeLocationBarModelDelegate {
 public:
  explicit LocationBarModelDelegateImpl(DocumentPipHost* host)
      : host_(CHECK_DEREF(host)) {}
  LocationBarModelDelegateImpl(const LocationBarModelDelegateImpl&) = delete;
  LocationBarModelDelegateImpl& operator=(const LocationBarModelDelegateImpl&) =
      delete;
  ~LocationBarModelDelegateImpl() override = default;

  // ChromeLocationBarModelDelegate:
  content::WebContents* GetActiveWebContents() const override {
    return host_->GetOpenerWebContents();
  }

 private:
  const raw_ref<DocumentPipHost> host_;
};

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

  // Build the LocationBarModel that drives the origin chip's URL and security
  // text from the opener WebContents, reusing the omnibox's formatting and
  // secure-display-text logic. Created before any UpdateOriginAndSecurity()
  // call (including a possible early OnThemeChanged()).
  location_bar_model_delegate_ =
      std::make_unique<LocationBarModelDelegateImpl>(&host_.get());
  location_bar_model_ = std::make_unique<LocationBarModelImpl>(
      location_bar_model_delegate_.get(), content::kMaxURLDisplayChars);

  // Create the top bar container.
  AddChildView(views::Builder<views::FlexLayoutView>()
                   .CopyAddressTo(&top_bar_container_view_)
                   .SetOrientation(views::LayoutOrientation::kHorizontal)
                   .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
                   .Build());

  top_bar_container_view_->SetBackground(
      views::CreateSolidBackground(kColorPipWindowTopBarBackground));

  // Create the origin chip: a security indicator (lock icon + optional
  // security chip text) that opens the Page Info dialog when pressed. Reuses
  // views::IconLabelBubbleView for geometry/ink-drop/animation parity with the
  // browser-backed frame's LocationIconView, but is driven from a
  // LocationBarModelImpl backed by the opener WebContents rather than the
  // omnibox stack. The URL/title is a separate sibling label outside the chip.
  const gfx::FontList& chip_font_list =
      views::TypographyProvider::Get().GetFont(CONTEXT_OMNIBOX_PRIMARY,
                                               views::style::STYLE_PRIMARY);
  auto origin_chip = std::make_unique<OriginChipView>(
      chip_font_list, /*delegate=*/this,
      base::BindRepeating(
          [](DocumentPipFrameView* frame_view) {
            return frame_view->ShowPageInfo();
          },
          // Safety: The widget owns the frame view and this chip, so the
          // callback cannot outlive the widget.
          base::Unretained(this)));
  origin_chip->SetProperty(views::kElementIdentifierKey,
                           kLocationIconElementId);
  origin_chip->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_TOOLTIP_LOCATION_ICON));
  origin_chip->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_TOOLTIP_LOCATION_ICON));
  // The external margin offsets the chip 8px from the window edge and 4px from
  // the window title, matching the browser-backed location icon.
  origin_chip->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(5, 8, 5, 4));
  origin_chip_ = top_bar_container_view_->AddChildView(std::move(origin_chip));

  // The window title label, showing the opener's URL. This is a sibling of the
  // origin chip (not a child), so it is excluded from the chip's Page Info
  // click target, matching the browser-backed frame's separate window-title
  // label. The gap from the chip is the chip's right margin.
  auto window_title = std::make_unique<views::Label>(
      std::u16string(), views::style::CONTEXT_LABEL,
      views::style::STYLE_PRIMARY);
  window_title->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  window_title->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kPreferred));
  window_title->SetBackgroundColor(kColorPipWindowTopBarBackground);
  window_title->SetEnabledColor(kColorPipWindowForeground);
  window_title_ =
      top_bar_container_view_->AddChildView(std::move(window_title));

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

  // Create the camera/microphone content-setting icons. These sit to the left
  // of the window-control buttons and surface the active media-capture state of
  // the opener WebContents. Mirrors PictureInPictureBrowserFrameView, but with
  // a null Browser (standalone PiP has no feature-promo surface).
  constexpr ContentSettingImageModel::ImageType kContentSettingImageOrder[] = {
      ContentSettingImageModel::ImageType::kMediaStream};
  const gfx::FontList& font_list = views::TypographyProvider::Get().GetFont(
      views::style::CONTEXT_LABEL, views::style::STYLE_PRIMARY);
  for (auto type : kContentSettingImageOrder) {
    auto model = ContentSettingImageModel::CreateForContentType(type);
    model->SetIconSize(kContentSettingIconSize);
    auto image_view = std::make_unique<ContentSettingImageView>(
        std::move(model), /*parent_delegate=*/this, /*delegate=*/this,
        /*browser=*/nullptr, font_list);
    image_view->SetProperty(views::kMarginsKey,
                            gfx::Insets::VH(kIconViewVerticalMargin, 0));
    image_view->SetBorder(views::CreateEmptyBorder(gfx::Insets(4)));
    content_setting_views_.push_back(
        button_container_view_->AddChildView(std::move(image_view)));
  }

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
    // Wrap the button in a fixed-size placeholder so its space stays reserved
    // in the layout while it is hidden. Browser-backed PiP keeps the
    // window-control buttons laid out (reserving their space) while inactive
    // and only hides them; toggling the button's own visibility would instead
    // collapse its space and let the window title grow. The wrapper always
    // reports the button's preferred size to the layout, so UpdateTopBarView()
    // can hide the button (cheaper than fading a compositor layer) while the
    // wrapper holds the space.
    auto back_to_tab_wrapper = std::make_unique<views::View>();
    back_to_tab_wrapper->SetUseDefaultFillLayout(true);
    back_to_tab_button_ =
        back_to_tab_wrapper->AddChildView(CreatePipTitleBarButton(
            back_icon,
            l10n_util::GetStringUTF16(
                IDS_PICTURE_IN_PICTURE_BACK_TO_TAB_CONTROL_TEXT),
            // Safety: The widget owns the frame view and its child buttons,
            // so the callback cannot outlive the widget.
            base::BindRepeating(back_to_tab_cb, base::Unretained(this))));
    back_to_tab_wrapper->SetPreferredSize(
        back_to_tab_button_->GetPreferredSize());
    button_container_view_->AddChildView(std::move(back_to_tab_wrapper));
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
  // Wrap the close button in a fixed-size placeholder for the same reason as
  // the back-to-tab button above: the wrapper reserves the button's space in
  // the layout so hiding the button while inactive doesn't let the window
  // title grow. See UpdateTopBarView().
  auto close_wrapper = std::make_unique<views::View>();
  close_wrapper->SetUseDefaultFillLayout(true);
  close_image_button_ = close_wrapper->AddChildView(CreatePipTitleBarButton(
      close_icon,
      l10n_util::GetStringUTF16(IDS_PICTURE_IN_PICTURE_CLOSE_CONTROL_TEXT),
      // Safety: The widget owns the frame view and its child buttons,
      // so the callback cannot outlive the widget.
      base::BindRepeating(close_cb, base::Unretained(this))));
  close_wrapper->SetPreferredSize(close_image_button_->GetPreferredSize());
  button_container_view_->AddChildView(std::move(close_wrapper));

  // If the window manager wants us to display an overlay, get it. In practice
  // this is the auto-PiP Allow / Block content-setting UI, shown when a site
  // enters PiP via the Auto Picture-in-Picture path. GetOverlayView() returns
  // null unless the opener has an AutoPictureInPictureTabHelper and auto-PiP
  // for the origin hasn't already been allowed/blocked/embargoed. Anchored to
  // the top bar and laid out over the contents area in Layout(). Mirrors
  // PictureInPictureBrowserFrameView.
  if (auto overlay =
          PictureInPictureWindowManager::GetInstance()->GetOverlayView(
              top_bar_container_view_, views::BubbleBorder::TOP_CENTER)) {
    auto_pip_setting_overlay_ = AddChildView(std::move(overlay));
  }

  // Clear the picture-in-picture window's cached bounds while the overlay is
  // visible, so the permission prompt isn't clipped by a smaller remembered
  // size.
  if (base::FeatureList::IsEnabled(
          media::kClearPipCachedBoundsWhenPermissionPromptVisible) &&
      IsOverlayViewVisible()) {
    PictureInPictureWindowManager::GetInstance()->ClearCachedBounds();
  }

  // TODO(crbug.com/40279642): Don't force dark mode once we support a
  // light mode window.
  widget->SetColorModeOverride(ui::ColorProviderKey::ColorMode::kDark);

  // Observe the widget for activation changes so the top bar can render its
  // active/inactive state. Narrowed to activation + destruction; window policy
  // (visibility, bounds, tucking) is owned by DocumentPipHost.
  widget_observation_.Observe(widget);

  // Seed the origin/title now. This is safe before the widget is realized: it
  // only sets label text and a ColorId-based security icon (whose color is
  // resolved lazily). The content-setting icons are seeded later in
  // AddedToWidget() instead, because refreshing an already-active icon (e.g.
  // camera in use when entering auto-PiP) paints it via GetForegroundColor() ->
  // GetColorProvider(), which is null until this view is attached to the
  // Widget.
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
  // Allow interacting with the origin chip (opens Page Info). The
  // window-title label next to it is draggable to match browser-backed PiP
  // behavior.
  if (GetOriginChipBounds().Contains(point)) {
    return HTCLIENT;
  }

  // Allow interacting with the buttons. Button bounds are interpreted in their
  // parent's coordinate space and converted to frame-view coordinates so the
  // hit-test comparisons share a coordinate space. The buttons stay laid out
  // while inactive (their fixed-size wrappers reserve the space) and are only
  // hidden, so gate on render_active_ to avoid hit-testing the hidden buttons.
  if (back_to_tab_button_ && render_active_ &&
      ConvertControlBoundsToFrame(back_to_tab_button_).Contains(point)) {
    return HTCLIENT;
  }
  if (render_active_ &&
      ConvertControlBoundsToFrame(close_image_button_).Contains(point)) {
    return HTCLIENT;
  }

  // Allow interacting with the visible content-setting icons (they open a
  // content-setting bubble when clicked).
  for (ContentSettingImageView* view : content_setting_views_) {
    if (view->GetVisible() &&
        ConvertControlBoundsToFrame(view).Contains(point)) {
      return HTCLIENT;
    }
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

  // The overlay covers the contents area below the top bar (not the chrome),
  // matching PictureInPictureBrowserFrameView.
  if (auto_pip_setting_overlay_) {
    auto_pip_setting_overlay_->SetBoundsRect(
        gfx::SubtractRects(content_area, top_bar));
  }

  LayoutSuperclass<views::FrameView>(this);
}

void DocumentPipFrameView::AddedToWidget() {
  views::FrameView::AddedToWidget();
  // Create the EventMonitor here (not in the ctor): it binds to the native
  // window, which is only guaranteed to exist once the view is attached to the
  // widget. Teardown is intentionally handled in OnWidgetDestroying rather
  // than in a symmetric RemovedFromWidget; see the comment there.
  window_event_observer_ = std::make_unique<WindowEventObserver>(this);

  // Seed the content-setting icons now that the Widget (and thus a
  // ColorProvider) exists. This is intentionally deferred from the ctor:
  // refreshing an already-visible icon paints it through
  // GetForegroundColor() -> GetColorProvider(), which is null before attach and
  // would crash when an icon is active at creation time (e.g. camera in use
  // when entering auto-PiP).
  UpdateContentSettingsIcons();

  // If the auto-PiP overlay is set, post a task to show it rather than showing
  // it inline, mirroring PictureInPictureBrowserFrameView::AddedToWidget. The
  // WeakPtr guards against the task firing after teardown.
  if (auto_pip_setting_overlay_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&DocumentPipFrameView::ShowOverlayIfNeeded,
                                  weak_factory_.GetWeakPtr()));
  }
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

void DocumentPipFrameView::UpdateWindowBoundsForRequestedInnerSize() {
  const blink::mojom::PictureInPictureWindowOptions& pip_options =
      host_->GetPipOptions();
  // Only a request that specifies an explicit inner width and height needs the
  // outer bounds recomputed. Without both, the manager's aspect-ratio fallback
  // already produced correct outer bounds at creation time.
  if (pip_options.width <= 0 || pip_options.height <= 0) {
    return;
  }

  views::Widget* const widget = GetWidget();
  CHECK(widget);

  // The platform (OS) border is the part of the outer window that the platform
  // reserves and chrome cannot draw into. It is only knowable once the Widget
  // and its native window exist, which is why the host invokes this after
  // Widget::Init() rather than at bounds-calculation time.
  const gfx::Size platform_border =
      widget->GetWindowBoundsInScreen().size() -
      widget->GetClientAreaBoundsInScreen().size();

  // The excluded margin is everything between the requested inner
  // (web-contents) size and the outer window size: our top bar + frame insets,
  // plus the platform border. Mirrors the computation in
  // PictureInPictureBrowserFrameView::OnBrowserViewInitialized.
  const gfx::Insets border_insets = FrameBorderInsets();
  const gfx::Size excluded_margin(
      border_insets.width() + platform_border.width(),
      GetTopAreaHeight() + border_insets.bottom() + platform_border.height());

  // GetMinimumSize() already includes the non-client area, so it is the minimum
  // *outer* window size expected by CalculateOuterWindowBounds.
  const gfx::Rect window_bounds =
      PictureInPictureWindowManager::GetInstance()->CalculateOuterWindowBounds(
          pip_options, GetMinimumSize(), excluded_margin);

  widget->SetBounds(window_bounds);
}

void DocumentPipFrameView::OnThemeChanged() {
  UpdateOriginAndSecurity();
  views::FrameView::OnThemeChanged();
}

///////////////////////////////////////////////////////////////////////////////
// views::WidgetObserver implementations:

void DocumentPipFrameView::OnWidgetActivationChanged(views::Widget* widget,
                                                     bool active) {
  UpdateTopBarView(active || mouse_inside_window_ || IsOverlayViewVisible());
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

  auto_pip_setting_overlay_ = nullptr;
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
  const GURL url = location_bar_model_->GetURL();

  // Reuse the omnibox's steady-state URL formatting (omit https://, trivial
  // subdomains, and, on desktop, the file:// scheme).
  const std::u16string url_text = location_bar_model_->GetURLForDisplay();
  window_title_->SetText(url_text);
  origin_chip_->GetViewAccessibility().SetDescription(url_text);

  // Pick the elision direction by scheme, mirroring
  // PictureInPictureBrowserFrameView. For file URLs we elide the tail, since
  // the file name/query can be crafted to look like an origin (spoofing). For
  // HTTPS URLs we elide the head, since everything but the origin is removed
  // for display, so a long origin could otherwise be pushed out of view.
  gfx::ElideBehavior elide_behavior =
      url.SchemeIsFile() ? gfx::ELIDE_TAIL : gfx::ELIDE_HEAD;
  // Extension and isolated-app URLs are like file URLs: the tail is the
  // spoofable part, so elide it.
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (url.SchemeIs(extensions::kExtensionScheme) ||
      url.SchemeIs(webapps::kIsolatedAppScheme)) {
    elide_behavior = gfx::ELIDE_TAIL;
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
  window_title_->SetElideBehavior(elide_behavior);

  const security_state::SecurityLevel security_level =
      location_bar_model_->GetSecurityLevel();
  const ui::ColorId foreground_color_id =
      render_active_ ? kColorPipWindowForeground
                     : kColorPipWindowForegroundInactive;

  auto* const chip = views::AsViewClass<OriginChipView>(origin_chip_);
  chip->SetSecurityImage(ui::ImageModel::FromVectorIcon(
      location_bar_model_->GetVectorIcon(), foreground_color_id,
      kSecurityIconImageSize));

  // Set the omnibox-style security chip text ("File"/extension/chrome
  // product/"Not secure"/"Dangerous") via the shared helper, animating the
  // change for the same level transitions the omnibox animates.
  const std::u16string chip_text = location_bar::GetSecurityChipText(
      location_bar_model_.get(), opener_web_contents,
      /*is_editing_or_empty=*/false);
  const bool animate =
      security_text_initialized_ &&
      location_bar::ShouldAnimateSecurityChipTextChange(
          /*is_editing_or_empty=*/false, last_security_level_, security_level);
  chip->UpdateSecurityText(chip_text, animate);
  last_security_level_ = security_level;
  security_text_initialized_ = true;
}

void DocumentPipFrameView::UpdateTopBarView(bool render_active) {
  if (render_active_ == render_active) {
    return;
  }
  render_active_ = render_active;

  // Browser-backed PiP keeps the window-control buttons laid out while
  // inactive (reserving their space) and only hides them. Each button lives in
  // a fixed-size wrapper that holds its space, so toggling the button's own
  // visibility keeps the origin/title labels from growing to fill the buttons'
  // space.
  if (back_to_tab_button_) {
    back_to_tab_button_->SetVisible(render_active_);
  }
  close_image_button_->SetVisible(render_active_);

  // TODO(crbug.com/515252142): Port the top-bar animations from
  // PictureInPictureBrowserFrameView::UpdateTopBarView (top-bar color fade,
  // camera-slide, and show/hide of the window-control buttons) for visual
  // parity. This currently applies the active/inactive color instantly instead
  // of animating. The animations are Browser-free, so porting them does not
  // reintroduce the //chrome/browser/ui monolith.
  const auto* color_provider = GetColorProvider();
  const SkColor foreground = color_provider->GetColor(
      render_active_ ? kColorPipWindowForeground
                     : kColorPipWindowForegroundInactive);
  window_title_->SetEnabledColor(foreground);
  UpdateOriginAndSecurity();
}

void DocumentPipFrameView::OnMouseEnteredOrExitedWindow(bool entered) {
  mouse_inside_window_ = entered;
  UpdateTopBarView(mouse_inside_window_ ||
                   (GetWidget() && GetWidget()->IsActive()) ||
                   IsOverlayViewVisible());
}

void DocumentPipFrameView::ShowOverlayIfNeeded() {
  if (auto_pip_setting_overlay_ && GetWidget()) {
    auto_pip_setting_overlay_->ShowBubble(GetWidget()->GetNativeView());
  }
}

bool DocumentPipFrameView::IsOverlayViewVisible() const {
  return auto_pip_setting_overlay_ && auto_pip_setting_overlay_->GetVisible();
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

///////////////////////////////////////////////////////////////////////////////
// IconLabelBubbleView::Delegate implementations:

SkColor DocumentPipFrameView::GetIconLabelBubbleSurroundingForegroundColor()
    const {
  return GetColorProvider()->GetColor(kColorPipWindowForeground);
}

SkColor DocumentPipFrameView::GetIconLabelBubbleBackgroundColor() const {
  return GetColorProvider()->GetColor(kColorPipWindowTopBarBackground);
}

///////////////////////////////////////////////////////////////////////////////
// ContentSettingImageViewDelegate implementations:

bool DocumentPipFrameView::ShouldHideContentSettingImage() {
  return false;
}

content::WebContents* DocumentPipFrameView::GetContentSettingWebContents() {
  // Mirror PictureInPictureBrowserFrameView: surface the content-setting state
  // of the opener WebContents, which holds the committed URL and page state.
  return host_->GetOpenerWebContents();
}

ContentSettingBubbleModelDelegate*
DocumentPipFrameView::GetContentSettingBubbleModelDelegate() {
  // TODO(crbug.com/515252142): Return a monolith-free bubble-model delegate
  // that routes the content-setting bubble's "Manage"/open-settings actions
  // through the opener WebContents (see issues/011 and Task 4e). Today this
  // returns null to avoid reintroducing the //chrome/browser/ui monolith
  // dependency (BrowserContentSettingBubbleModelDelegate). The only surfaced
  // content setting (kMediaStream) null-checks the delegate, so the bubble
  // still opens but its "Manage media settings" action is inert — a parity
  // gap vs. PictureInPictureBrowserFrameView that must be closed.
  return nullptr;
}

void DocumentPipFrameView::UpdateContentSettingsIcons() {
  // TODO(crbug.com/515252142): For parity with
  // PictureInPictureBrowserFrameView::UpdateContentSettingsIcons, re-apply a
  // visibility-dependent margin on `button_container_view_` so the spacing
  // between the content-setting icons and the window controls stays consistent
  // when the camera/mic icon appears or disappears (coupled to the camera-slide
  // animation).
  for (ContentSettingImageView* view : content_setting_views_) {
    view->Update();
  }
}

BEGIN_METADATA(DocumentPipFrameView)
END_METADATA
