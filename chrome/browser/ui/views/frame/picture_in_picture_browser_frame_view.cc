// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/picture_in_picture_browser_frame_view.h"

#include <memory>

#include "base/containers/to_vector.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_occlusion_tracker.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/ui/browser_content_setting_bubble_model_delegate.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_init_state.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/overlay/overlay_window_image_button.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_specification.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"
#include "chrome/browser/ui/views/picture_in_picture/picture_in_picture_bounds_change_animation.h"
#include "chrome/browser/ui/views/picture_in_picture/picture_in_picture_tucker.h"
#include "chrome/browser/ui/views/picture_in_picture/pip_top_bar_animation_controller.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/location_bar_model_impl.h"
#include "components/vector_icons/vector_icons.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "components/webapps/isolated_web_apps/scheme.h"
#include "content/public/browser/document_picture_in_picture_window_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_constants.h"
#include "extensions/buildflags/buildflags.h"
#include "media/base/media_switches.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/layer.h"
#include "ui/display/screen.h"
#include "ui/events/event_observer.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/event_monitor.h"
#include "ui/views/layout/animating_layout_manager.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/frame_background.h"
#include "ui/views/window/window_shape.h"

#if BUILDFLAG(IS_WIN)
#include "ui/base/win/hwnd_metrics.h"
#include "ui/views/win/hwnd_util.h"
#endif

#if !BUILDFLAG(IS_MAC)
// Mac does not use Aura
#include "ui/aura/window.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/constants.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
namespace {

constexpr int kWindowIconImageSize = 16;
constexpr int kBackToTabImageSize = 16;
constexpr int kContentSettingIconSize = 16;

// The height of the controls bar at the top of the window.
constexpr int kTopControlsHeight = 34;
// The vertical margin for IconLabelBubbleView to have 24px height.
constexpr int KIconViewVerticalMargin = 5;

constexpr int kResizeBorder = 10;
constexpr int kResizeAreaCornerSize = 16;

class BackToTabButton : public OverlayWindowImageButton {
  METADATA_HEADER(BackToTabButton, OverlayWindowImageButton)

 public:
  explicit BackToTabButton(PressedCallback callback)
      : OverlayWindowImageButton(std::move(callback)) {
    auto* icon = &(features::IsRoundedIconsEnabled()
                       ? vector_icons::kBackToTabIcon
                       : vector_icons::kBackToTabChromeRefreshOldIcon);
    SetImageModel(views::Button::STATE_NORMAL,
                  ui::ImageModel::FromVectorIcon(
                      *icon, kColorPipWindowForeground, kBackToTabImageSize));

    const std::u16string back_to_tab_button_label = l10n_util::GetStringUTF16(
        IDS_PICTURE_IN_PICTURE_BACK_TO_TAB_CONTROL_TEXT);
    SetTooltipText(back_to_tab_button_label);
  }
  BackToTabButton(const BackToTabButton&) = delete;
  BackToTabButton& operator=(const BackToTabButton&) = delete;
  ~BackToTabButton() override = default;
};

BEGIN_METADATA(BackToTabButton)
END_METADATA

// Helper class for observing mouse and key events from native window.
class WindowEventObserver : public ui::EventObserver {
 public:
  explicit WindowEventObserver(
      PictureInPictureBrowserFrameView* pip_browser_frame_view)
      : pip_browser_frame_view_(pip_browser_frame_view) {
    event_monitor_ = views::EventMonitor::CreateWindowMonitor(
        this, pip_browser_frame_view_->GetWidget()->GetNativeWindow(),
        {ui::EventType::kMouseMoved, ui::EventType::kMouseExited,
         ui::EventType::kKeyPressed, ui::EventType::kKeyReleased});
  }

  WindowEventObserver(const WindowEventObserver&) = delete;
  WindowEventObserver& operator=(const WindowEventObserver&) = delete;
  ~WindowEventObserver() override = default;

  void OnEvent(const ui::Event& event) override {
    if (event.IsKeyEvent()) {
      pip_browser_frame_view_->UpdateTopBarView(true);
      return;
    }

    // TODO(crbug.com/40883490): Windows doesn't capture mouse exit event
    // sometimes when mouse leaves the window.
    // TODO(jazzhsu): We are checking if mouse is in bounds rather than strictly
    // checking mouse enter/exit event because of two reasons: 1. We are getting
    // mouse exit/enter events when mouse moves between client and non-client
    // area on Linux and Windows; 2. We will get a mouse exit event when a
    // context menu is brought up. This might cause the pip window stuck in the
    // "in" state when some other window is on top of the pip window.
    pip_browser_frame_view_->OnMouseEnteredOrExitedWindow(IsMouseInBounds());
  }

 private:
  bool IsMouseInBounds() {
    gfx::Point point = event_monitor_->GetLastMouseLocation();
    views::View::ConvertPointFromScreen(pip_browser_frame_view_, &point);

    gfx::Rect hit_region = pip_browser_frame_view_->GetHitRegion();
    return hit_region.Contains(point);
  }

  raw_ptr<PictureInPictureBrowserFrameView> pip_browser_frame_view_;
  std::unique_ptr<views::EventMonitor> event_monitor_;
};

void DefinitelyExitPictureInPicture(
    PictureInPictureBrowserFrameView& frame_view,
    PictureInPictureWindowManager::UiBehavior behavior) {
  switch (behavior) {
    case PictureInPictureWindowManager::UiBehavior::kCloseWindowOnly:
    case PictureInPictureWindowManager::UiBehavior::kCloseWindowAndPauseVideo:
      frame_view.set_close_reason(
          PictureInPictureBrowserFrameView::CloseReason::kCloseButton);
      break;
    case PictureInPictureWindowManager::UiBehavior::kCloseWindowAndFocusOpener:
      frame_view.set_close_reason(
          PictureInPictureBrowserFrameView::CloseReason::kBackToTabButton);
      break;
  }
  if (!PictureInPictureWindowManager::GetInstance()
           ->ExitPictureInPictureViaWindowUi(behavior)) {
    // If the picture-in-picture controller has been disconnected for
    // some reason, then just manually close the window to prevent
    // getting into a state where the back to tab button no longer
    // closes the window.
    frame_view.GetBrowserView()->Close();
  }
}

}  // namespace

PictureInPictureBrowserFrameView::PictureInPictureBrowserFrameView(
    BrowserWidget* widget,
    BrowserView* browser_view)
    : BrowserFrameView(widget, browser_view) {
  // We create our own top container, so we hide the one created by default (and
  // its children) from the user and accessibility tools.
  browser_view->top_container()->SetVisible(false);
  browser_view->top_container()->SetEnabled(false);
  browser_view->top_container()->GetViewAccessibility().SetIsIgnored(true);
  browser_view->top_container()->GetViewAccessibility().SetIsLeaf(true);

  location_bar_model_ = std::make_unique<LocationBarModelImpl>(
      this, content::kMaxURLDisplayChars);

  // Creates a view for the top bar area.
  AddChildView(views::Builder<views::FlexLayoutView>()
                   .CopyAddressTo(&top_bar_container_view_)
                   .SetOrientation(views::LayoutOrientation::kHorizontal)
                   .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
                   .Build());

  top_bar_container_view_->SetBackground(
      views::CreateSolidBackground(kColorPipWindowTopBarBackground));

  // Creates the window icon.
  const gfx::FontList& font_list = views::TypographyProvider::Get().GetFont(
      CONTEXT_OMNIBOX_PRIMARY, views::style::STYLE_PRIMARY);
  location_icon_view_ = top_bar_container_view_->AddChildView(
      std::make_unique<LocationIconView>(font_list, this, this));
  // The PageInfo icon should be 8px from the left of the window and 4px from
  // the right of the origin. Meanwhile, it should have vertical margins set to
  // keep the hover-over highlight circular.
  location_icon_view_->SetProperty(
      views::kMarginsKey, gfx::Insets::TLBR(KIconViewVerticalMargin, 8,
                                            KIconViewVerticalMargin, 4));

  // For file URLs, we want to elide the tail, since the file name and/or query
  // part of the file URL can be made to look like an origin for spoofing. For
  // HTTPS URLs, we elide the head to prevent spoofing via long origins, since
  // in the HTTPS case everything besides the origin is removed for display.
  auto elide_behavior = location_bar_model_->GetURL().SchemeIsFile()
                            ? gfx::ELIDE_TAIL
                            : gfx::ELIDE_HEAD;

  // Similarly for extension URLs and isolated-app URLs, the tail is more
  // important to elide.
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (location_bar_model_->GetURL().SchemeIs(extensions::kExtensionScheme) ||
      location_bar_model_->GetURL().SchemeIs(webapps::kIsolatedAppScheme)) {
    elide_behavior = gfx::ELIDE_TAIL;
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  // TODO(crbug.com/424715850): use IWA app name in title (plus why registrar
  // based on browser_view->GetProfile doesn't know about the app).

  // Creates the window title.
  top_bar_container_view_->AddChildView(
      views::Builder<views::Label>(
          std::make_unique<views::Label>(
              location_bar_model_->GetURLForDisplay(),
              views::style::CONTEXT_LABEL, views::style::STYLE_PRIMARY,
              gfx::DirectionalityMode::DIRECTIONALITY_AS_URL))
          .CopyAddressTo(&window_title_)
          .SetHorizontalAlignment(gfx::ALIGN_LEFT)
          .SetElideBehavior(elide_behavior)
          .SetProperty(
              views::kFlexBehaviorKey,
              views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                                       views::MinimumFlexSizeRule::kScaleToZero,
                                       views::MaximumFlexSizeRule::kUnbounded))
          .Build());

  window_title_->SetBackgroundColor(kColorPipWindowTopBarBackground);
  window_title_->SetEnabledColor(kColorPipWindowForeground);

  // Creates a container view for the top right buttons to handle the button
  // animations.
  button_container_view_ = top_bar_container_view_->AddChildView(
      std::make_unique<views::FlexLayoutView>());

  // Creates the content setting models. Currently we only support camera and
  // microphone settings.
  constexpr ContentSettingImageModel::ImageType kContentSettingImageOrder[] = {
      ContentSettingImageModel::ImageType::kMediaStream};
  std::vector<std::unique_ptr<ContentSettingImageModel>> models;
  for (auto type : kContentSettingImageOrder) {
    models.push_back(ContentSettingImageModel::CreateForContentType(type));
  }

  // Creates the content setting views based on the models.
  for (auto& model : models) {
    model->SetIconSize(kContentSettingIconSize);
    auto image_view = std::make_unique<ContentSettingImageView>(
        std::move(model), this, this, browser_view->browser(), font_list);

    // The ContentSettingImageView should have vertical margins set to keep the
    // hover-over highlight circular. Otherwise, the highlight will occupy the
    // full height of the top control.
    image_view->SetProperty(views::kMarginsKey,
                            gfx::Insets::VH(KIconViewVerticalMargin, 0));
    // Adjust internal padding on each side to 4px to ensure a min size of
    // 24x24, consistent with other icon views. The default paddings are
    // narrower.
    image_view->SetBorder(views::CreateEmptyBorder((gfx::Insets(4))));

    content_setting_views_.push_back(
        button_container_view_->AddChildView(std::move(image_view)));
  }

  // Creates the back to tab button if one should be shown based on the given
  // PictureInPictureWindowOptions. If the options don't exist (this can happen
  // in some test situations), then default to displaying the back to tab
  // button.
  const std::optional<blink::mojom::PictureInPictureWindowOptions> pip_options =
      browser_view->GetDocumentPictureInPictureOptions();
  if (!pip_options.has_value() || !pip_options->disallow_return_to_opener) {
    back_to_tab_button_ = button_container_view_->AddChildView(
        std::make_unique<BackToTabButton>(base::BindRepeating(
            [](PictureInPictureBrowserFrameView* frame_view) {
              DefinitelyExitPictureInPicture(
                  *frame_view, PictureInPictureWindowManager::UiBehavior::
                                   kCloseWindowAndFocusOpener);
            },
            base::Unretained(this))));
  }

  // Creates the close button.
  close_image_button_ = button_container_view_->AddChildView(
      std::make_unique<CloseImageButton>(base::BindRepeating(
          [](PictureInPictureBrowserFrameView* frame_view) {
            DefinitelyExitPictureInPicture(
                *frame_view,
                PictureInPictureWindowManager::UiBehavior::kCloseWindowOnly);
          },
          base::Unretained(this))));

  // Creates the controller that owns and drives the top-bar hover animations,
  // now that the buttons and content-setting views it references exist. It
  // paints the window-control buttons to layers so their opacity can be
  // animated, and starts in the active state. Mirrors DocumentPipFrameView.
  animation_controller_ = std::make_unique<PipTopBarAnimationController>(
      this, back_to_tab_button_, close_image_button_, content_setting_views_);

  // If the window manager wants us to display an overlay, get it.  In practice,
  // this is the auto-pip Allow / Block content setting UI.
  if (auto auto_pip_setting_overlay =
          PictureInPictureWindowManager::GetInstance()->GetOverlayView(
              top_bar_container_view_, views::BubbleBorder::TOP_CENTER)) {
    auto_pip_setting_overlay_ =
        AddChildView(std::move(auto_pip_setting_overlay));
  }

  // Clear the picture-in-picture window cached bounds, whenever the
  // `auto_pip_setting_overlay_` is visible.
  if (base::FeatureList::IsEnabled(
          media::kClearPipCachedBoundsWhenPermissionPromptVisible) &&
      IsOverlayViewVisible()) {
    PictureInPictureWindowManager::GetInstance()->ClearCachedBounds();
  }
}

PictureInPictureBrowserFrameView::~PictureInPictureBrowserFrameView() {
  base::UmaHistogramEnumeration("Media.DocumentPictureInPicture.CloseReason",
                                close_reason_);
  PictureInPictureWindowManager::GetInstance()->OnPictureInPictureWindowHidden(
      this);
}

///////////////////////////////////////////////////////////////////////////////
// BrowserFrameView implementations:

bool PictureInPictureBrowserFrameView::ShouldShowWebAppFrameToolbar() const {
  return false;
}

int PictureInPictureBrowserFrameView::GetTopInset(bool restored) const {
  return GetTopAreaHeight();
}

void PictureInPictureBrowserFrameView::ShowOverlayIfNeeded() {
  if (auto_pip_setting_overlay_ && GetWidget()) {
    auto_pip_setting_overlay_->ShowBubble(GetWidget()->GetNativeView());
  }
}

void PictureInPictureBrowserFrameView::OnBrowserViewInitViewsComplete() {
  BrowserFrameView::OnBrowserViewInitViewsComplete();

#if BUILDFLAG(IS_WIN)
  const gfx::Insets insets = GetClientAreaInsets(
      MonitorFromWindow(HWNDForView(this), MONITOR_DEFAULTTONEAREST));
#else
  const gfx::Insets insets;
#endif

  const std::optional<blink::mojom::PictureInPictureWindowOptions> pip_options =
      GetBrowserView()->GetDocumentPictureInPictureOptions();

  // If the request includes pip options with an inner width and height, then we
  // need to recompute the outer size now that we can compute the correct
  // margin.  While we know how much space we will reserve for the title bar,
  // etc., we do not know how much the platform window will reserve until we
  // have a Widget and can ask it.  Since we now have a Widget, do that.
  if (!pip_options.has_value() || pip_options->width <= 0 ||
      pip_options->height <= 0) {
    // Request didn't specify a width and height -- whatever's fine!
    return;
  }

  // Convert the inner bounds in the request to outer bounds.  Note that the
  // bounds cache might make all of this work wasted; it caches the outer size
  // directly.  In that case, the excluded margin we compute won't be used, and
  // probably the browser coordinates are already correct, but that's fine.

  // Compute the margin required by both the platform and the browser frame
  // (us) to provide the requested inner size.

  // This is the area that is included in the outer size that chrome doesn't
  // get to use.  This is called the "client area" of the widget, but it's
  // different than what we call the client area.  The former client is chrome,
  // while the latter client is just the part inside the frame that we draw.
  const auto platform_border =
      GetWidget()->GetWindowBoundsInScreen().size() -
      GetWidget()->GetClientAreaBoundsInScreen().size();
  // Add the amount we reserve inside the platform borders to get the total
  // difference between the inner and outer size.
  gfx::Size excluded_margin(
      FrameBorderInsets().width() + platform_border.width(),
      GetTopAreaHeight() + FrameBorderInsets().bottom() +
          platform_border.height());

  // Remember that this might ignore the pip options if the bounds cache
  // provides the correct outer size.  This is fine; `excluded_margin` will
  // simply be ignored and nothing will change.
  const gfx::Rect window_bounds =
      PictureInPictureWindowManager::GetInstance()->CalculateOuterWindowBounds(
          pip_options.value(),
          GetMinimumSize() + gfx::Size(insets.width(), insets.height()),
          excluded_margin);

  BrowserInitState::From(GetBrowserView()->browser())
      ->set_override_bounds(window_bounds);
}

gfx::Rect PictureInPictureBrowserFrameView::GetBoundsForClientView() const {
  auto border_thickness = FrameBorderInsets();
  int top_height = GetTopAreaHeight();
  return gfx::Rect(border_thickness.left(), top_height,
                   width() - border_thickness.width(),
                   height() - top_height - border_thickness.bottom());
}

gfx::Rect PictureInPictureBrowserFrameView::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  auto border_thickness = FrameBorderInsets();
  int top_height = GetTopAreaHeight();
  return gfx::Rect(
      client_bounds.x() - border_thickness.left(),
      client_bounds.y() - top_height,
      client_bounds.width() + border_thickness.width(),
      client_bounds.height() + top_height + border_thickness.bottom());
}

int PictureInPictureBrowserFrameView::NonClientHitTest(
    const gfx::Point& point) {
  // Allow interacting with the buttons.
  if (GetLocationIconViewBounds().Contains(point) ||
      GetBackToTabControlsBounds().Contains(point) ||
      GetCloseControlsBounds().Contains(point)) {
    return HTCLIENT;
  }

  for (size_t i = 0; i < content_setting_views_.size(); i++) {
    if (GetContentSettingViewBounds(i).Contains(point)) {
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
  int frame_component =
      browser_widget()->client_view()->NonClientHitTest(point);
  if (frame_component != HTNOWHERE) {
    return frame_component;
  }

  return HTCAPTION;
}

void PictureInPictureBrowserFrameView::GetWindowMask(const gfx::Size& size,
                                                     SkPath* window_mask) {
  DCHECK(window_mask);
  views::GetDefaultWindowMask(size, window_mask);
}

void PictureInPictureBrowserFrameView::UpdateWindowIcon() {
  // This will be called after WebContents in PictureInPictureWindowManager is
  // set, so that we can update the icon and title based on WebContents.
  location_icon_view_->Update(/*suppress_animations=*/false);
  window_title_->SetText(location_bar_model_->GetURLForDisplay());
}

// Minimum size refers to the minimum size for the window inner bounds.
gfx::Size PictureInPictureBrowserFrameView::GetMinimumSize() const {
  const gfx::Size minimum(
      PictureInPictureWindowManager::GetMinimumInnerWindowSize() +
      GetNonClientViewAreaSize());
  return minimum;
}

gfx::Size PictureInPictureBrowserFrameView::GetMaximumSize() const {
  if (!GetWidget() || !GetWidget()->GetNativeWindow()) {
    // The maximum size can't be smaller than the minimum size.
    return GetMinimumSize();
  }

  auto display = display::Screen::Get()->GetDisplayNearestWindow(
      GetWidget()->GetNativeWindow());
  return PictureInPictureWindowManager::GetMaximumWindowSize(display);
}

void PictureInPictureBrowserFrameView::OnThemeChanged() {
  const auto* color_provider = GetColorProvider();
  for (ContentSettingImageView* view : content_setting_views_) {
    view->SetIconColor(color_provider->GetColor(kColorPipWindowForeground));
  }

  BrowserFrameView::OnThemeChanged();
}

void PictureInPictureBrowserFrameView::Layout(PassKey) {
  gfx::Rect content_area = GetLocalBounds();
  content_area.Inset(FrameBorderInsets());
  gfx::Rect top_bar = content_area;
  top_bar.set_height(kTopControlsHeight);
  top_bar_container_view_->SetBoundsRect(top_bar);
#if !BUILDFLAG(IS_ANDROID)
  if (auto_pip_setting_overlay_) {
    auto_pip_setting_overlay_->SetBoundsRect(
        gfx::SubtractRects(content_area, top_bar));
  }
#endif

  LayoutSuperclass<BrowserFrameView>(this);
}

void PictureInPictureBrowserFrameView::AddedToWidget() {
  widget_observation_.Observe(GetWidget());
  window_event_observer_ = std::make_unique<WindowEventObserver>(this);
  child_dialog_observer_helper_ =
      std::make_unique<PipChildDialogObserverHelper>(this);

  // Attach the top-bar animations to a compositor-backed container so they
  // update together and in sync with the display. Requires the Widget, so it
  // is deferred from the ctor to here. Mirrors DocumentPipFrameView.
  animation_controller_->SetUpAnimationContainer(GetWidget());

  // TODO(crbug.com/40279642): Don't force dark mode once we support a
  // light mode window.
  GetWidget()->SetColorModeOverride(ui::ColorProviderKey::ColorMode::kDark);

// Fade in animation is disabled for Document and Video Picture-in-Picture on
// Windows. On Windows, resizable windows can not be translucent. See
// crbug.com/425711450.
#if !BUILDFLAG(IS_WIN)
  if (!fade_animator_) {
    fade_animator_ = std::make_unique<PictureInPictureWidgetFadeAnimator>();
  }
  fade_animator_->AnimateShowWindow(
      GetWidget(), PictureInPictureWidgetFadeAnimator::WidgetShowType::kNone);
#endif

  // If the AutoPiP setting overlay is set, then post a task to show it.  Don't
  // do this here, since not all observers might have found out about the new
  // widget yet.  Specifically, on cros, the BrowserViewAsh immediately does a
  // layout if we have to resize, which causes it to assume that it has a widget
  // and crash.  I don't know if this is because resizing at this point is bad,
  // or if the Ash browser view is broken, but either way waiting until the dust
  // settles a bit makes sense.
  if (auto_pip_setting_overlay_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&PictureInPictureBrowserFrameView::ShowOverlayIfNeeded,
                       weak_factory_.GetWeakPtr()));
  }

  PictureInPictureOcclusionTracker* tracker =
      PictureInPictureWindowManager::GetInstance()->GetOcclusionTracker();
  if (tracker) {
    tracker->OnPictureInPictureWidgetOpened(GetWidget());
  }

  PictureInPictureWindowManager::GetInstance()->OnPictureInPictureWindowShown(
      this);

  BrowserFrameView::AddedToWidget();
}

void PictureInPictureBrowserFrameView::RemovedFromWidget() {
  widget_observation_.Reset();
  window_event_observer_.reset();
  child_dialog_observer_helper_.reset();

  // Clear the AutoPiP setting overlay view.
  if (auto_pip_setting_overlay_) {
    auto_pip_setting_overlay_ = nullptr;
  }

  if (fade_animator_) {
    fade_animator_->CancelAndReset();
  }

  PictureInPictureWindowManager::GetInstance()->OnPictureInPictureWindowHidden(
      this);
  tucker_.reset();

  BrowserFrameView::RemovedFromWidget();
}

void PictureInPictureBrowserFrameView::SetFrameBounds(const gfx::Rect& bounds) {
  gfx::Rect adjusted_bounds(bounds);
  gfx::Rect current_bounds = GetWidget()->GetWindowBoundsInScreen();
  bool did_adjust_size = false;

  auto display = display::Screen::Get()->GetDisplayNearestWindow(
      GetWidget()->GetNativeWindow());

  // If the website is requesting that the window increases in size, then ensure
  // that it's not increasing beyond the site-requested maximum.
  if (bounds.size().width() > current_bounds.size().width() ||
      bounds.size().height() > current_bounds.size().height()) {
    gfx::Size adjusted_new_size =
        PictureInPictureWindowManager::AdjustRequestedSizeIfNecessary(
            bounds.size(), display);

    // If so, then use the adjusted size centered on the current location rather
    // than centered on the new location (as we only ever expect size to change,
    // and a large requested size could incidentally move the window).
    if (adjusted_new_size != bounds.size()) {
      adjusted_bounds = current_bounds;
      adjusted_bounds.ToCenteredSize(adjusted_new_size);

      // Ensure the bounds are fully within the display work area.
      adjusted_bounds.AdjustToFit(display.work_area());

      did_adjust_size = true;
    }
  }

  base::UmaHistogramBoolean(
      "Media.DocumentPictureInPicture.RequestedLargeResize", did_adjust_size);

  if (!base::FeatureList::IsEnabled(
          media::kDocumentPictureInPictureAnimateResize) ||
      !gfx::Animation::ShouldRenderRichAnimation() || is_tucking_forced_) {
    BrowserFrameView::SetFrameBounds(adjusted_bounds);

    // If we're forced to tuck, then re-tuck after the size adjustment. Note
    // that we also always skip the bounds change animation when tucking is
    // forced.
    if (is_tucking_forced_) {
      tucker_->Tuck();
    }
    return;
  }
  bounds_change_animation_ =
      std::make_unique<PictureInPictureBoundsChangeAnimation>(*browser_widget(),
                                                              adjusted_bounds);
  bounds_change_animation_->Start();
}

///////////////////////////////////////////////////////////////////////////////
// views::FrameView implementations:
gfx::Rect
PictureInPictureBrowserFrameView::GetNonDecoratedClientAreaBoundsInScreen()
    const {
  return GetBoundsInScreen();
}

///////////////////////////////////////////////////////////////////////////////
// ChromeLocationBarModelDelegate implementations:

content::WebContents* PictureInPictureBrowserFrameView::GetActiveWebContents()
    const {
  return PictureInPictureWindowManager::GetInstance()->GetWebContents();
}

bool PictureInPictureBrowserFrameView::GetURL(GURL* url) const {
  DCHECK(url);
  if (GetActiveWebContents()) {
    *url = GetActiveWebContents()->GetLastCommittedURL();
    return true;
  }
  return false;
}

bool PictureInPictureBrowserFrameView::ShouldPreventElision() {
  // We should never allow the full URL to show, as the PiP window only cares
  // about the origin of the opener.
  return false;
}

bool PictureInPictureBrowserFrameView::ShouldTrimDisplayUrlAfterHostName()
    const {
  // We need to set the window title URL to be eTLD+1.
  return true;
}

bool PictureInPictureBrowserFrameView::ShouldDisplayURL() const {
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// LocationIconView::Delegate implementations:

content::WebContents* PictureInPictureBrowserFrameView::GetWebContents() {
  return PictureInPictureWindowManager::GetInstance()->GetWebContents();
}

bool PictureInPictureBrowserFrameView::IsEditingOrEmpty() const {
  return false;
}

SkColor PictureInPictureBrowserFrameView::GetSecurityChipColor(
    security_state::SecurityLevel security_level) const {
  return GetColorProvider()->GetColor(kColorOmniboxSecurityChipSecure);
}

bool PictureInPictureBrowserFrameView::ShowPageInfoDialog() {
  content::WebContents* contents = GetWebContents();
  if (!contents) {
    return false;
  }

  std::unique_ptr<PageInfoBubbleSpecification> specification =
      PageInfoBubbleSpecification::Builder(
          views::BubbleAnchor(location_icon_view_),
          GetWidget()->GetNativeWindow(), contents,
          contents->GetLastCommittedURL())
          .HideExtendedSiteInfo()
          .Build();

  views::BubbleDialogDelegateView* const bubble =
      PageInfoBubbleView::CreatePageInfoBubble(std::move(specification));
  bubble->SetHighlightedElement(kLocationIconElementId);
  bubble->GetWidget()->Show();

  PictureInPictureOcclusionTracker* tracker =
      PictureInPictureWindowManager::GetInstance()->GetOcclusionTracker();
  if (tracker) {
    tracker->OnPictureInPictureWidgetOpened(bubble->GetWidget());
  }

  return true;
}

LocationBarModel* PictureInPictureBrowserFrameView::GetLocationBarModel()
    const {
  return location_bar_model_.get();
}

ui::ImageModel PictureInPictureBrowserFrameView::GetLocationIcon(
    LocationIconView::Delegate::IconFetchedCallback on_icon_fetched) {
  // If we're animating between colors, use the current color value.
  if (const auto current_foreground_color =
          animation_controller_->current_foreground_color();
      current_foreground_color.has_value()) {
    return ui::ImageModel::FromVectorIcon(location_bar_model_->GetVectorIcon(),
                                          *current_foreground_color,
                                          kWindowIconImageSize);
  }

  ui::ColorId foreground_color_id = animation_controller_->is_top_bar_active()
                                        ? kColorPipWindowForeground
                                        : kColorPipWindowForegroundInactive;

  return ui::ImageModel::FromVectorIcon(location_bar_model_->GetVectorIcon(),
                                        foreground_color_id,
                                        kWindowIconImageSize);
}

std::optional<ui::ColorId>
PictureInPictureBrowserFrameView::GetLocationIconBackgroundColorOverride()
    const {
  return kColorPipWindowTopBarBackground;
}

///////////////////////////////////////////////////////////////////////////////
// IconLabelBubbleView::Delegate implementations:

SkColor
PictureInPictureBrowserFrameView::GetIconLabelBubbleSurroundingForegroundColor()
    const {
  return GetColorProvider()->GetColor(kColorPipWindowForeground);
}

SkColor PictureInPictureBrowserFrameView::GetIconLabelBubbleBackgroundColor()
    const {
  return GetColorProvider()->GetColor(kColorPipWindowTopBarBackground);
}

///////////////////////////////////////////////////////////////////////////////
// ContentSettingImageView::Delegate implementations:

bool PictureInPictureBrowserFrameView::ShouldHideContentSettingImage() {
  return false;
}

content::WebContents*
PictureInPictureBrowserFrameView::GetContentSettingWebContents() {
  // Use the opener web contents for content settings since it has full info
  // such as last committed URL, etc. that are called to be used.
  return GetWebContents();
}

ContentSettingBubbleModelDelegate*
PictureInPictureBrowserFrameView::GetContentSettingBubbleModelDelegate() {
  // Use the opener browser delegate to open any new tab.
  BrowserWindowInterface* browser =
      GlobalBrowserCollection::GetInstance()->FindBrowserWithTab(
          GetWebContents());
  return browser->GetFeatures().content_setting_bubble_model_delegate();
}

///////////////////////////////////////////////////////////////////////////////
// views::WidgetObserver implementations:

void PictureInPictureBrowserFrameView::OnWidgetActivationChanged(
    views::Widget* widget,
    bool active) {
  // The window may become inactive when a popup modal shows, so we need to
  // check if the mouse is still inside the window.
  UpdateTopBarView(active || mouse_inside_window_ || IsOverlayViewVisible());
}

void PictureInPictureBrowserFrameView::OnWidgetDestroying(
    views::Widget* widget) {
  window_event_observer_.reset();
  widget_observation_.Reset();
  child_dialog_observer_helper_.reset();
}

void PictureInPictureBrowserFrameView::OnWidgetVisibilityChanged(
    views::Widget* widget,
    bool visible) {
  if (visible) {
    EnforceTucking();
  }
}

void PictureInPictureBrowserFrameView::OnUserDesiredBoundsChanged(
    const gfx::Rect& bounds) {
  CHECK(GetWidget());
  const auto pip_display = display::Screen::Get()->GetDisplayNearestWindow(
      GetWidget()->GetNativeWindow());

  // The cached bounds update is driven by the `PipChildDialogObserverHelper`
  // helper, because it is the only place where we track the user-desired bounds
  // (filtering out dialog resizes and rounding noise). In the future, we might
  // want to split the PiP window bounds tracking into a separate component to
  // decouple it from child dialog observation (or generalize the child dialog
  // observation).
  PictureInPictureWindowManager::GetInstance()->UpdateCachedBounds(bounds,
                                                                   pip_display);
}

///////////////////////////////////////////////////////////////////////////////
// PictureInPictureWindow implementations:

void PictureInPictureBrowserFrameView::SetForcedTucking(bool tuck) {
  if (!tucker_) {
    CHECK(GetWidget());
    tucker_ = std::make_unique<PictureInPictureTucker>(*GetWidget());
  }
  is_tucking_forced_ = tuck;

  // Attempting to tuck our Widget before it's been shown causes issues since
  // it may be still adjusting its bounds. Once visible, tucking will be
  // enforced.
  if (GetWidget()->IsVisible()) {
    EnforceTucking();
  }
}

#if BUILDFLAG(IS_MAC)
void PictureInPictureBrowserFrameView::OnAnyBrowserEnteredFullscreen() {
  GetWidget()->MoveToActiveFullscreenSpace();
}
#endif  // BUILDFLAG(IS_MAC)

void PictureInPictureBrowserFrameView::EnforceTucking() {
  // The `tucker_` will have been created if there's any tucking to be enforced.
  if (!tucker_) {
    return;
  }

  if (is_tucking_forced_) {
    // Stop any existing bounds change animations.
    if (bounds_change_animation_) {
      bounds_change_animation_->End();
    }
    tucker_->Tuck();
  } else {
    tucker_->Untuck();
  }
}

///////////////////////////////////////////////////////////////////////////////
// PipTopBarAnimationController::Delegate implementations:

void PictureInPictureBrowserFrameView::ApplyTopBarForegroundColor(
    SkColor color) {
  window_title_->SetEnabledColor(color);
  for (ContentSettingImageView* view : content_setting_views_) {
    view->SetIconColor(color);
  }

  // NOTE: This handles what was previously
  // PictureInPictureBrowserFrameView::AnimationEnded/AnimationProgressed
  // updating the window icon directly.
  location_icon_view_->Update(/*suppress_animations=*/false);
}

const ui::ColorProvider*
PictureInPictureBrowserFrameView::GetTopBarColorProvider() const {
  return GetColorProvider();
}

///////////////////////////////////////////////////////////////////////////////
// PictureInPictureBrowserFrameView implementations:

gfx::Rect PictureInPictureBrowserFrameView::GetHitRegion() const {
  return GetLocalBounds();
}

gfx::Rect PictureInPictureBrowserFrameView::ConvertTopBarControlViewBounds(
    views::View* control_view,
    views::View* source_view) const {
  gfx::RectF bounds(control_view->GetMirroredBounds());
  views::View::ConvertRectToTarget(source_view, this, &bounds);
  return gfx::ToEnclosingRect(bounds);
}

gfx::Rect PictureInPictureBrowserFrameView::GetLocationIconViewBounds() const {
  DCHECK(location_icon_view_);
  return ConvertTopBarControlViewBounds(location_icon_view_,
                                        top_bar_container_view_);
}

gfx::Rect PictureInPictureBrowserFrameView::GetContentSettingViewBounds(
    size_t index) const {
  DCHECK(index < content_setting_views_.size());
  return ConvertTopBarControlViewBounds(content_setting_views_[index],
                                        button_container_view_);
}

gfx::Rect PictureInPictureBrowserFrameView::GetBackToTabControlsBounds() const {
  if (!back_to_tab_button_) {
    return gfx::Rect();
  }
  return ConvertTopBarControlViewBounds(back_to_tab_button_,
                                        button_container_view_);
}

gfx::Rect PictureInPictureBrowserFrameView::GetCloseControlsBounds() const {
  DCHECK(close_image_button_);
  return ConvertTopBarControlViewBounds(close_image_button_,
                                        button_container_view_);
}

LocationIconView* PictureInPictureBrowserFrameView::GetLocationIconView() {
  return location_icon_view_;
}

void PictureInPictureBrowserFrameView::UpdateContentSettingsIcons() {
  const auto kButtonContainerViewWithCameraButtonInsets = gfx::Insets::TLBR(
      0, 0, 0, GetLayoutConstant(LayoutConstant::kTabAfterTitlePadding));
  const auto kButtonContainerViewInsets = gfx::Insets::VH(
      0, GetLayoutConstant(LayoutConstant::kTabAfterTitlePadding));

  for (ContentSettingImageView* view : content_setting_views_) {
    view->Update();

    // Currently the only content setting view we have is for camera and
    // microphone settings, and we add margin insets based on its visibility to
    // the button container view to be consistent with the normal browser
    // window.
    button_container_view_->SetProperty(
        views::kMarginsKey,
        (view->GetVisible() ? kButtonContainerViewWithCameraButtonInsets
                            : kButtonContainerViewInsets));
  }
}

void PictureInPictureBrowserFrameView::UpdateTopBarView(bool render_active) {
  // Pass the target state into the controller and let it handle overlapping
  // start/stop transitions and animations automatically. Mirrors
  // DocumentPipFrameView.
  animation_controller_->SetTopBarActiveStatus(render_active);
}

gfx::Insets PictureInPictureBrowserFrameView::ResizeBorderInsets() const {
  return gfx::Insets(kResizeBorder);
}

gfx::Insets PictureInPictureBrowserFrameView::FrameBorderInsets() const {
  return gfx::Insets();
}

int PictureInPictureBrowserFrameView::GetTopAreaHeight() const {
  return FrameBorderInsets().top() + kTopControlsHeight;
}

gfx::Size PictureInPictureBrowserFrameView::GetNonClientViewAreaSize() const {
  const auto border_thickness = FrameBorderInsets();
  const int top_height = GetTopAreaHeight();

  return gfx::Size(border_thickness.width(),
                   top_height + border_thickness.bottom());
}

#if BUILDFLAG(IS_WIN)
gfx::Insets PictureInPictureBrowserFrameView::GetClientAreaInsets(
    HMONITOR monitor) const {
  const int frame_thickness = ui::GetResizableFrameThicknessFromMonitorInPixels(
      monitor, /*has_caption=*/true);
  return gfx::Insets::TLBR(0, frame_thickness, frame_thickness,
                           frame_thickness);
}
#endif

bool PictureInPictureBrowserFrameView::HasAnyVisibleContentSettingViews()
    const {
  for (ContentSettingImageView* view : content_setting_views_) {
    if (view->GetVisible()) {
      return true;
    }
  }
  return false;
}

// Helper functions for testing.
std::vector<raw_ptr<gfx::Animation>>
PictureInPictureBrowserFrameView::GetRenderActiveAnimationsForTesting() {
  return base::ToVector(
      animation_controller_
          ->GetActiveTransitionAnimationsForTesting(),  // IN-TEST
      [](gfx::Animation* animation) {
        return raw_ptr<gfx::Animation>(animation);
      });
}

std::vector<raw_ptr<gfx::Animation>>
PictureInPictureBrowserFrameView::GetRenderInactiveAnimationsForTesting() {
  return base::ToVector(
      animation_controller_
          ->GetInactiveTransitionAnimationsForTesting(),  // IN-TEST
      [](gfx::Animation* animation) {
        return raw_ptr<gfx::Animation>(animation);
      });
}

views::View* PictureInPictureBrowserFrameView::GetBackToTabButtonForTesting() {
  return back_to_tab_button_;
}

views::View* PictureInPictureBrowserFrameView::GetCloseButtonForTesting() {
  return close_image_button_;
}

views::Label* PictureInPictureBrowserFrameView::GetWindowTitleForTesting() {
  return window_title_;
}

void PictureInPictureBrowserFrameView::SetWindowTitleForTesting(  // IN-TEST
    const std::u16string& title) {
  CHECK(window_title_);
  window_title_->SetText(title);
}

PictureInPictureWidgetFadeAnimator*
PictureInPictureBrowserFrameView::GetFadeAnimatorForTesting() {
  return fade_animator_.get();
}

void PictureInPictureBrowserFrameView::OnMouseEnteredOrExitedWindow(
    bool entered) {
  mouse_inside_window_ = entered;
  // If the overlay view is visible, then we should keep the top bar icons
  // visible too.  If the overlay is dismissed, we'll leave it in the same state
  // until a mouse-out event, which is reasonable.  If the UI is dismissed via
  // the mouse, then it's inside the window anyway.  If it's dismissed via the
  // keyboard, keeping it that way until the next mouse in/out actually looks
  // better than having the top bar hide immediately.
  UpdateTopBarView(mouse_inside_window_ || IsOverlayViewVisible());
}

bool PictureInPictureBrowserFrameView::IsOverlayViewVisible() const {
  return auto_pip_setting_overlay_ && auto_pip_setting_overlay_->GetVisible();
}

gfx::Size PictureInPictureBrowserFrameView::ComputeDialogPadding() const {
  auto* host = GetBrowserView()->GetWebContentsModalDialogHost();
  if (!host) {
    return gfx::Size();
  }

  // This is not guaranteed, but should be fairly robust if the maximum dialog
  // size computation changes.  It also prevents us from memorizing how all of
  // it works.
  return GetWidget()->GetSize() - host->GetMaximumDialogSize();
}

views::Widget* PictureInPictureBrowserFrameView::GetPipWidget() {
  return GetWidget();
}

BEGIN_METADATA(PictureInPictureBrowserFrameView)
END_METADATA
