// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/picture_in_picture_browser_frame_view.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/ui/browser_content_setting_bubble_model_delegate.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/overlay/overlay_window_image_button.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/location_bar_model_impl.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_constants.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/compositor/layer.h"
#include "ui/display/screen.h"
#include "ui/events/event_observer.h"
#include "ui/gfx/animation/animation_container.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/compositor_animation_runner.h"
#include "ui/views/event_monitor.h"
#include "ui/views/layout/animating_layout_manager.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/frame_background.h"
#include "ui/views/window/window_shape.h"

#if !BUILDFLAG(IS_MAC)
// Mac does not use Aura
#include "ui/aura/window.h"
#endif

#if BUILDFLAG(IS_LINUX)
#include "chrome/browser/ui/views/frame/browser_frame_view_paint_utils_linux.h"
#include "chrome/browser/ui/views/frame/desktop_browser_frame_aura_linux.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/wm/window_util.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/ui/frame/interior_resize_handler_targeter.h"
#endif

namespace {

constexpr int kWindowIconImageSize = 16;
constexpr int kBackToTabImageSize = 16;

// The height of the controls bar at the top of the window.
constexpr int kTopControlsHeight = 30;

#if BUILDFLAG(IS_LINUX)
// Frame border when window shadow is not drawn.
constexpr int kFrameBorderThickness = 4;
#endif

constexpr int kResizeBorder = 10;
constexpr int kResizeAreaCornerSize = 16;

// The time duration that the top bar animation will take in total.
constexpr base::TimeDelta kAnimationDuration = base::Milliseconds(250);

// The animation durations for the top right buttons, which are separated into
// multiple parts because some changes need to be delayed.
constexpr std::array<base::TimeDelta, 2>
    kMoveCameraButtonToRightAnimationDurations = {kAnimationDuration * 0.4,
                                                  kAnimationDuration * 0.6};
constexpr std::array<base::TimeDelta, 3>
    kShowBackToTabButtonAnimationDurations = {kAnimationDuration * 0.4,
                                              kAnimationDuration * 0.4,
                                              kAnimationDuration * 0.2};
constexpr std::array<base::TimeDelta, 2>
    kHideBackToTabButtonAnimationDurations = {kAnimationDuration * 0.4,
                                              kAnimationDuration * 0.6};
constexpr std::array<base::TimeDelta, 3> kCloseButtonAnimationDurations = {
    kAnimationDuration * 0.2, kAnimationDuration * 0.4,
    kAnimationDuration * 0.4};

class BackToTabButton : public OverlayWindowImageButton {
 public:
  METADATA_HEADER(BackToTabButton);

  explicit BackToTabButton(PressedCallback callback)
      : OverlayWindowImageButton(std::move(callback)) {
    SetImageModel(views::Button::STATE_NORMAL,
                  ui::ImageModel::FromVectorIcon(vector_icons::kBackToTabIcon,
                                                 kColorPipWindowForeground,
                                                 kBackToTabImageSize));

    const std::u16string back_to_tab_button_label = l10n_util::GetStringUTF16(
        IDS_PICTURE_IN_PICTURE_BACK_TO_TAB_CONTROL_TEXT);
    SetTooltipText(back_to_tab_button_label);
  }
  BackToTabButton(const BackToTabButton&) = delete;
  BackToTabButton& operator=(const BackToTabButton&) = delete;
  ~BackToTabButton() override = default;
};

BEGIN_METADATA(BackToTabButton, OverlayWindowImageButton)
END_METADATA

// Helper class for observing mouse and key events from native window.
class WindowEventObserver : public ui::EventObserver {
 public:
  explicit WindowEventObserver(
      PictureInPictureBrowserFrameView* pip_browser_frame_view)
      : pip_browser_frame_view_(pip_browser_frame_view) {
    event_monitor_ = views::EventMonitor::CreateWindowMonitor(
        this, pip_browser_frame_view_->GetWidget()->GetNativeWindow(),
        {ui::ET_MOUSE_MOVED, ui::ET_MOUSE_EXITED, ui::ET_KEY_PRESSED,
         ui::ET_KEY_RELEASED});
  }

  WindowEventObserver(const WindowEventObserver&) = delete;
  WindowEventObserver& operator=(const WindowEventObserver&) = delete;
  ~WindowEventObserver() override = default;

  void OnEvent(const ui::Event& event) override {
    if (event.IsKeyEvent()) {
      pip_browser_frame_view_->UpdateTopBarView(true);
      return;
    }

    // TODO(crbug.com/1400085): Windows doesn't capture mouse exit event
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

    gfx::Rect input_bounds = pip_browser_frame_view_->GetLocalBounds();

#if BUILDFLAG(IS_LINUX)
    // Calculate input bounds for Linux. This is needed because the input bounds
    // is not necessary the same as the local bounds on Linux.
    if (pip_browser_frame_view_->ShouldDrawFrameShadow()) {
      gfx::Insets insets = pip_browser_frame_view_->MirroredFrameBorderInsets();
      const auto tiled_edges = pip_browser_frame_view_->frame()->tiled_edges();
      if (tiled_edges.left)
        insets.set_left(0);
      if (tiled_edges.right)
        insets.set_right(0);
      if (tiled_edges.top)
        insets.set_top(0);
      if (tiled_edges.bottom)
        insets.set_bottom(0);

      input_bounds.Inset(insets + pip_browser_frame_view_->GetInputInsets());
    }
#endif

    return input_bounds.Contains(point);
  }

  raw_ptr<PictureInPictureBrowserFrameView> pip_browser_frame_view_;
  std::unique_ptr<views::EventMonitor> event_monitor_;
};

void DefinitelyExitPictureInPicture(
    PictureInPictureBrowserFrameView& frame_view) {
  if (!PictureInPictureWindowManager::GetInstance()->ExitPictureInPicture()) {
    // If the picture-in-picture controller has been disconnected for
    // some reason, then just manually close the window to prevent
    // getting into a state where the back to tab button no longer
    // closes the window.
    frame_view.browser_view()->Close();
  }
}

}  // namespace

PictureInPictureBrowserFrameView::PictureInPictureBrowserFrameView(
    BrowserFrame* frame,
    BrowserView* browser_view)
    : BrowserNonClientFrameView(frame, browser_view),
      top_bar_color_animation_(this),
      move_camera_button_to_left_animation_(this),
      move_camera_button_to_right_animation_(
          std::vector<gfx::MultiAnimation::Part>{
              gfx::MultiAnimation::Part(
                  kMoveCameraButtonToRightAnimationDurations[0],
                  gfx::Tween::Type::ZERO,
                  1.0,
                  1.0),
              gfx::MultiAnimation::Part(
                  kMoveCameraButtonToRightAnimationDurations[1],
                  gfx::Tween::Type::EASE_OUT,
                  1.0,
                  0.0)}),
      show_back_to_tab_button_animation_(std::vector<gfx::MultiAnimation::Part>{
          gfx::MultiAnimation::Part(kShowBackToTabButtonAnimationDurations[0],
                                    gfx::Tween::Type::ZERO,
                                    0.0,
                                    0.0),
          gfx::MultiAnimation::Part(kShowBackToTabButtonAnimationDurations[1],
                                    gfx::Tween::Type::LINEAR,
                                    0.0,
                                    1.0),
          gfx::MultiAnimation::Part(kShowBackToTabButtonAnimationDurations[2],
                                    gfx::Tween::Type::ZERO,
                                    1.0,
                                    1.0)}),
      hide_back_to_tab_button_animation_(std::vector<gfx::MultiAnimation::Part>{
          gfx::MultiAnimation::Part(kHideBackToTabButtonAnimationDurations[0],
                                    gfx::Tween::Type::LINEAR,
                                    1.0,
                                    0.0),
          gfx::MultiAnimation::Part(kHideBackToTabButtonAnimationDurations[1],
                                    gfx::Tween::Type::ZERO,
                                    0.0,
                                    0.0)}),
      show_close_button_animation_(std::vector<gfx::MultiAnimation::Part>{
          gfx::MultiAnimation::Part(kCloseButtonAnimationDurations[0],
                                    gfx::Tween::Type::ZERO,
                                    0.0,
                                    0.0),
          gfx::MultiAnimation::Part(kCloseButtonAnimationDurations[1],
                                    gfx::Tween::Type::LINEAR,
                                    0.0,
                                    1.0),
          gfx::MultiAnimation::Part(kCloseButtonAnimationDurations[2],
                                    gfx::Tween::Type::ZERO,
                                    1.0,
                                    1.0)}),
      hide_close_button_animation_(std::vector<gfx::MultiAnimation::Part>{
          gfx::MultiAnimation::Part(kCloseButtonAnimationDurations[0],
                                    gfx::Tween::Type::ZERO,
                                    1.0,
                                    1.0),
          gfx::MultiAnimation::Part(kCloseButtonAnimationDurations[1],
                                    gfx::Tween::Type::LINEAR,
                                    1.0,
                                    0.0),
          gfx::MultiAnimation::Part(kCloseButtonAnimationDurations[2],
                                    gfx::Tween::Type::ZERO,
                                    0.0,
                                    0.0)}) {
  // We create our own top container, so we hide the one created by default (and
  // its children) from the user and accessibility tools.
  browser_view->top_container()->SetVisible(false);
  browser_view->top_container()->SetEnabled(false);
  browser_view->top_container()->GetViewAccessibility().OverrideIsIgnored(true);
  browser_view->top_container()->GetViewAccessibility().OverrideIsLeaf(true);

  location_bar_model_ = std::make_unique<LocationBarModelImpl>(
      this, content::kMaxURLDisplayChars);

  // Creates a view for the top bar area.
  AddChildView(views::Builder<views::FlexLayoutView>()
                   .CopyAddressTo(&top_bar_container_view_)
                   .SetOrientation(views::LayoutOrientation::kHorizontal)
                   .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
                   .Build());

  // Creates the window icon.
  const gfx::FontList& font_list = views::style::GetFont(
      CONTEXT_OMNIBOX_PRIMARY, views::style::STYLE_PRIMARY);
  location_icon_view_ = top_bar_container_view_->AddChildView(
      std::make_unique<LocationIconView>(font_list, this, this));

  // Creates the window title.
  top_bar_container_view_->AddChildView(
      views::Builder<views::Label>()
          .CopyAddressTo(&window_title_)
          .SetText(location_bar_model_->GetURLForDisplay())
          .SetHorizontalAlignment(gfx::ALIGN_LEFT)
          .SetElideBehavior(gfx::ELIDE_HEAD)
          .SetProperty(
              views::kFlexBehaviorKey,
              views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                                       views::MaximumFlexSizeRule::kUnbounded))
          .Build());

  // Creates a container view for the top right buttons to handle the button
  // animations.
  button_container_view_ = top_bar_container_view_->AddChildView(
      std::make_unique<views::FlexLayoutView>());

  // Creates the content setting models. Currently we only support camera and
  // microphone settings.
  constexpr ContentSettingImageModel::ImageType kContentSettingImageOrder[] = {
      ContentSettingImageModel::ImageType::MEDIASTREAM};
  std::vector<std::unique_ptr<ContentSettingImageModel>> models;
  for (auto type : kContentSettingImageOrder)
    models.push_back(ContentSettingImageModel::CreateForContentType(type));

  // Creates the content setting views based on the models.
  for (auto& model : models) {
    auto image_view = std::make_unique<ContentSettingImageView>(
        std::move(model), this, this, font_list);
    content_setting_views_.push_back(
        button_container_view_->AddChildView(std::move(image_view)));
  }

  // Creates the back to tab button.
  back_to_tab_button_ = button_container_view_->AddChildView(
      std::make_unique<BackToTabButton>(base::BindRepeating(
          [](PictureInPictureBrowserFrameView* frame_view) {
            PictureInPictureWindowManager::GetInstance()->FocusInitiator();
            DefinitelyExitPictureInPicture(*frame_view);
          },
          base::Unretained(this))));

  // Creates the close button.
  close_image_button_ = button_container_view_->AddChildView(
      std::make_unique<CloseImageButton>(base::BindRepeating(
          [](PictureInPictureBrowserFrameView* frame_view) {
            DefinitelyExitPictureInPicture(*frame_view);
          },
          base::Unretained(this))));

  // Enable button layer rendering to set opacity for animation.
  back_to_tab_button_->SetPaintToLayer();
  back_to_tab_button_->layer()->SetFillsBoundsOpaquely(false);
  close_image_button_->SetPaintToLayer();
  close_image_button_->layer()->SetFillsBoundsOpaquely(false);

  // Creates the top bar title color and camera icon color animation. Set the
  // initial state to 1.0 because the window is active when first shown.
  top_bar_color_animation_.SetSlideDuration(kAnimationDuration);
  top_bar_color_animation_.SetTweenType(gfx::Tween::LINEAR);
  top_bar_color_animation_.Reset(1.0);

  // Creates the camera icon movement animations with the default EASE_OUT type.
  move_camera_button_to_left_animation_.SetSlideDuration(kAnimationDuration);
  move_camera_button_to_right_animation_.set_continuous(false);
  move_camera_button_to_right_animation_.set_delegate(this);

  // Creates the button animations.
  show_back_to_tab_button_animation_.set_continuous(false);
  show_back_to_tab_button_animation_.set_delegate(this);
  hide_back_to_tab_button_animation_.set_continuous(false);
  hide_back_to_tab_button_animation_.set_delegate(this);
  show_close_button_animation_.set_continuous(false);
  show_close_button_animation_.set_delegate(this);
  hide_close_button_animation_.set_continuous(false);
  hide_close_button_animation_.set_delegate(this);

#if BUILDFLAG(IS_LINUX)
  frame_background_ = std::make_unique<views::FrameBackground>();
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::window_util::InstallResizeHandleWindowTargeterForWindow(
      frame->GetNativeWindow());
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  frame->GetNativeWindow()->SetEventTargeter(
      std::make_unique<chromeos::InteriorResizeHandleTargeter>());
#endif
}

PictureInPictureBrowserFrameView::~PictureInPictureBrowserFrameView() = default;

///////////////////////////////////////////////////////////////////////////////
// BrowserNonClientFrameView implementations:

gfx::Rect PictureInPictureBrowserFrameView::GetBoundsForTabStripRegion(
    const gfx::Size& tabstrip_minimum_size) const {
  return gfx::Rect();
}

gfx::Rect PictureInPictureBrowserFrameView::GetBoundsForWebAppFrameToolbar(
    const gfx::Size& toolbar_preferred_size) const {
  return gfx::Rect();
}

void PictureInPictureBrowserFrameView::LayoutWebAppWindowTitle(
    const gfx::Rect& available_space,
    views::Label& window_title_label) const {}

int PictureInPictureBrowserFrameView::GetTopInset(bool restored) const {
  return GetTopAreaHeight();
}

int PictureInPictureBrowserFrameView::GetThemeBackgroundXInset() const {
  return 0;
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
  // Do nothing if the click is outside the window.
  if (!GetLocalBounds().Contains(point))
    return HTNOWHERE;

  // Allow interacting with the buttons.
  if (GetLocationIconViewBounds().Contains(point) ||
      GetBackToTabControlsBounds().Contains(point) ||
      GetCloseControlsBounds().Contains(point))
    return HTCLIENT;

  for (size_t i = 0; i < content_setting_views_.size(); i++) {
    if (GetContentSettingViewBounds(i).Contains(point))
      return HTCLIENT;
  }

  // Allow dragging and resizing the window.
  int window_component = GetHTComponentForFrame(
      point, ResizeBorderInsets(), kResizeAreaCornerSize, kResizeAreaCornerSize,
      GetWidget()->widget_delegate()->CanResize());
  if (window_component != HTNOWHERE)
    return window_component;

  // Allow interacting with the web contents.
  int frame_component = frame()->client_view()->NonClientHitTest(point);
  if (frame_component != HTNOWHERE)
    return frame_component;

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
  return PictureInPictureWindowManager::GetMinimumInnerWindowSize() +
         GetNonClientViewAreaSize();
}

gfx::Size PictureInPictureBrowserFrameView::GetMaximumSize() const {
  if (!GetWidget() || !GetWidget()->GetNativeWindow())
    return gfx::Size();

  auto display = display::Screen::GetScreen()->GetDisplayNearestWindow(
      GetWidget()->GetNativeWindow());
  return PictureInPictureWindowManager::GetMaximumWindowSize(display);
}

void PictureInPictureBrowserFrameView::OnThemeChanged() {
  const auto* color_provider = GetColorProvider();
  window_title_->SetBackgroundColor(
      color_provider->GetColor(kColorPipWindowTopBarBackground));
  window_title_->SetEnabledColor(
      color_provider->GetColor(kColorPipWindowForeground));
  for (ContentSettingImageView* view : content_setting_views_)
    view->SetIconColor(color_provider->GetColor(kColorPipWindowForeground));

#if !BUILDFLAG(IS_LINUX)
  // On Linux the top bar background will be drawn in OnPaint().
  top_bar_container_view_->SetBackground(views::CreateSolidBackground(
      color_provider->GetColor(kColorPipWindowTopBarBackground)));
#endif

  BrowserNonClientFrameView::OnThemeChanged();
}

void PictureInPictureBrowserFrameView::Layout() {
  auto border_thickness = FrameBorderInsets();
  top_bar_container_view_->SetBoundsRect(
      gfx::Rect(border_thickness.left(), border_thickness.top(),
                width() - border_thickness.width(), kTopControlsHeight));

  BrowserNonClientFrameView::Layout();
}

void PictureInPictureBrowserFrameView::AddedToWidget() {
  widget_observation_.Observe(GetWidget());
  window_event_observer_ = std::make_unique<WindowEventObserver>(this);

  // Creates an animation container to ensure all the animations update at the
  // same time.
  gfx::AnimationContainer* animation_container = new gfx::AnimationContainer();
  animation_container->SetAnimationRunner(
      std::make_unique<views::CompositorAnimationRunner>(GetWidget()));
  top_bar_color_animation_.SetContainer(animation_container);
  move_camera_button_to_left_animation_.SetContainer(animation_container);
  move_camera_button_to_right_animation_.SetContainer(animation_container);
  show_back_to_tab_button_animation_.SetContainer(animation_container);
  hide_back_to_tab_button_animation_.SetContainer(animation_container);
  show_close_button_animation_.SetContainer(animation_container);
  hide_close_button_animation_.SetContainer(animation_container);

  BrowserNonClientFrameView::AddedToWidget();
}

void PictureInPictureBrowserFrameView::RemovedFromWidget() {
  widget_observation_.Reset();
  window_event_observer_.reset();

  BrowserNonClientFrameView::RemovedFromWidget();
}

#if BUILDFLAG(IS_LINUX)
gfx::Insets PictureInPictureBrowserFrameView::MirroredFrameBorderInsets()
    const {
  auto border = FrameBorderInsets();
  return base::i18n::IsRTL() ? gfx::Insets::TLBR(border.top(), border.right(),
                                                 border.bottom(), border.left())
                             : border;
}

gfx::Insets PictureInPictureBrowserFrameView::GetInputInsets() const {
  return gfx::Insets(ShouldDrawFrameShadow() ? -kResizeBorder : 0);
}

SkRRect PictureInPictureBrowserFrameView::GetRestoredClipRegion() const {
  gfx::RectF bounds_dip(GetLocalBounds());
  if (ShouldDrawFrameShadow()) {
    gfx::InsetsF border(MirroredFrameBorderInsets());
    bounds_dip.Inset(border);
  }

  float radius_dip = 0;
  if (window_frame_provider_) {
    radius_dip = window_frame_provider_->GetTopCornerRadiusDip();
  } else {
    radius_dip = ChromeLayoutProvider::Get()->GetCornerRadiusMetric(
        views::Emphasis::kHigh);
  }
  SkVector radii[4]{{radius_dip, radius_dip}, {radius_dip, radius_dip}, {}, {}};
  SkRRect clip;
  clip.setRectRadii(gfx::RectFToSkRect(bounds_dip), radii);
  return clip;
}
#endif

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
  if (!contents)
    return false;

  views::BubbleDialogDelegateView* bubble =
      PageInfoBubbleView::CreatePageInfoBubble(
          location_icon_view_, gfx::Rect(), GetWidget()->GetNativeWindow(),
          contents, contents->GetLastCommittedURL(),
          /*initialized_callback=*/base::DoNothing(),
          /*closing_callback=*/base::DoNothing());
  bubble->SetHighlightedButton(location_icon_view_);
  bubble->GetWidget()->Show();
  return true;
}

LocationBarModel* PictureInPictureBrowserFrameView::GetLocationBarModel()
    const {
  return location_bar_model_.get();
}

ui::ImageModel PictureInPictureBrowserFrameView::GetLocationIcon(
    LocationIconView::Delegate::IconFetchedCallback on_icon_fetched) const {
  return ui::ImageModel::FromVectorIcon(location_bar_model_->GetVectorIcon(),
                                        kColorOmniboxSecurityChipSecure,
                                        kWindowIconImageSize);
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
  Browser* browser = chrome::FindBrowserWithWebContents(GetWebContents());
  return browser->content_setting_bubble_model_delegate();
}

///////////////////////////////////////////////////////////////////////////////
// views::WidgetObserver implementations:

void PictureInPictureBrowserFrameView::OnWidgetActivationChanged(
    views::Widget* widget,
    bool active) {
  // The window may become inactive when a popup modal shows, so we need to
  // check if the mouse is still inside the window.
  if (!active && mouse_inside_window_)
    active = true;
  UpdateTopBarView(active);
}

void PictureInPictureBrowserFrameView::OnWidgetDestroying(
    views::Widget* widget) {
  window_event_observer_.reset();
  widget_observation_.Reset();
}

///////////////////////////////////////////////////////////////////////////////
// gfx::AnimationDelegate implementations:

void PictureInPictureBrowserFrameView::AnimationProgressed(
    const gfx::Animation* animation) {
  if (animation == &top_bar_color_animation_) {
    SkColor color = gfx::Tween::ColorValueBetween(
        animation->GetCurrentValue(),
        GetColorProvider()->GetColor(kColorPipWindowForegroundInactive),
        GetColorProvider()->GetColor(kColorPipWindowForeground));
    window_title_->SetEnabledColor(color);
    for (ContentSettingImageView* view : content_setting_views_) {
      view->SetIconColor(color);
    }
    return;
  }

  if (animation == &move_camera_button_to_left_animation_ ||
      animation == &move_camera_button_to_right_animation_) {
    for (ContentSettingImageView* view : content_setting_views_) {
      // Set the position of camera icon relative to |button_container_view_|.
      view->SetX(animation->CurrentValueBetween(
          back_to_tab_button_->width() + close_image_button_->width(), 0));
    }
    return;
  }

  if (animation == &show_back_to_tab_button_animation_ ||
      animation == &hide_back_to_tab_button_animation_) {
    back_to_tab_button_->layer()->SetOpacity(animation->GetCurrentValue());
    return;
  }

  CHECK(animation == &show_close_button_animation_ ||
        animation == &hide_close_button_animation_);
  close_image_button_->layer()->SetOpacity(animation->GetCurrentValue());
}

///////////////////////////////////////////////////////////////////////////////
// views::View implementations:

void PictureInPictureBrowserFrameView::OnPaint(gfx::Canvas* canvas) {
#if BUILDFLAG(IS_LINUX)
  // Draw the PiP window frame borders and shadows, including the top bar
  // background.
  if (window_frame_provider_) {
    window_frame_provider_->PaintWindowFrame(
        canvas, GetLocalBounds(), GetTopAreaHeight(), ShouldPaintAsActive(),
        frame()->tiled_edges());
  } else {
    DCHECK(frame_background_);
    frame_background_->set_frame_color(
        GetColorProvider()->GetColor(kColorPipWindowTopBarBackground));
    frame_background_->set_use_custom_frame(frame()->UseCustomFrame());
    frame_background_->set_is_active(ShouldPaintAsActive());
    frame_background_->set_theme_image(GetFrameImage());
    frame_background_->set_theme_image_y_inset(
        ThemeProperties::kFrameHeightAboveTabs - GetTopAreaHeight());
    frame_background_->set_theme_overlay_image(GetFrameOverlayImage());
    frame_background_->set_top_area_height(GetTopAreaHeight());
    PaintRestoredFrameBorderLinux(
        *canvas, *this, frame_background_.get(), GetRestoredClipRegion(),
        ShouldDrawFrameShadow(), MirroredFrameBorderInsets(),
        GetShadowValues());
  }
#endif
  BrowserNonClientFrameView::OnPaint(canvas);
}

///////////////////////////////////////////////////////////////////////////////
// PictureInPictureBrowserFrameView implementations:

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
  DCHECK(back_to_tab_button_);
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
  const auto kButtonContainerViewWithCameraButtonInsets =
      gfx::Insets::TLBR(0, 0, 0, GetLayoutConstant(TAB_AFTER_TITLE_PADDING));
  const auto kButtonContainerViewInsets =
      gfx::Insets::VH(0, GetLayoutConstant(TAB_AFTER_TITLE_PADDING));

  for (auto* view : content_setting_views_) {
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
  // Check if the update is needed to avoid redundant animations.
  if (render_active_ == render_active) {
    return;
  }

  render_active_ = render_active;

  // Stop the previous animations since if this function is called too soon,
  // previous animations may override the new animations.
  if (render_active_) {
    move_camera_button_to_right_animation_.Stop();
    hide_back_to_tab_button_animation_.Stop();
    hide_close_button_animation_.Stop();

    top_bar_color_animation_.Show();

    // SlideAnimation needs to be reset if only Show() is called.
    move_camera_button_to_left_animation_.Reset(0.0);
    move_camera_button_to_left_animation_.Show();

    show_back_to_tab_button_animation_.Start();
    show_close_button_animation_.Start();
  } else {
    move_camera_button_to_left_animation_.Stop();
    show_back_to_tab_button_animation_.Stop();
    show_close_button_animation_.Stop();

    top_bar_color_animation_.Hide();
    move_camera_button_to_right_animation_.Start();
    hide_back_to_tab_button_animation_.Start();
    hide_close_button_animation_.Start();
  }
}

gfx::Insets PictureInPictureBrowserFrameView::FrameBorderInsets() const {
#if BUILDFLAG(IS_LINUX)
  if (window_frame_provider_) {
    const auto insets = window_frame_provider_->GetFrameThicknessDip();
    const auto tiled_edges = frame()->tiled_edges();

    // If edges of the window are tiled and snapped to the edges of the desktop,
    // window_frame_provider_ will skip drawing.
    return gfx::Insets::TLBR(tiled_edges.top ? 0 : insets.top(),
                             tiled_edges.left ? 0 : insets.left(),
                             tiled_edges.bottom ? 0 : insets.bottom(),
                             tiled_edges.right ? 0 : insets.right());
  }
  return GetRestoredFrameBorderInsetsLinux(
      ShouldDrawFrameShadow(), gfx::Insets(kFrameBorderThickness),
      frame()->tiled_edges(), GetShadowValues(), kResizeBorder);
#else
  return gfx::Insets();
#endif
}

gfx::Insets PictureInPictureBrowserFrameView::ResizeBorderInsets() const {
#if BUILDFLAG(IS_LINUX)
  return FrameBorderInsets();
#else
  return gfx::Insets(kResizeBorder);
#endif
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

#if BUILDFLAG(IS_LINUX)
void PictureInPictureBrowserFrameView::SetWindowFrameProvider(
    ui::WindowFrameProvider* window_frame_provider) {
  DCHECK(window_frame_provider);
  window_frame_provider_ = window_frame_provider;

  // Only one of window_frame_provider_ and frame_background_ will be used.
  frame_background_.reset();
}

bool PictureInPictureBrowserFrameView::ShouldDrawFrameShadow() const {
  return static_cast<DesktopBrowserFrameAuraLinux*>(
             frame()->native_browser_frame())
      ->ShouldDrawRestoredFrameShadow();
}

// static
gfx::ShadowValues PictureInPictureBrowserFrameView::GetShadowValues() {
  int elevation = ChromeLayoutProvider::Get()->GetShadowElevationMetric(
      views::Emphasis::kMaximum);
  return gfx::ShadowValue::MakeMdShadowValues(elevation);
}
#endif

// Helper functions for testing.
std::vector<gfx::Animation*>
PictureInPictureBrowserFrameView::GetRenderActiveAnimationsForTesting() {
  return std::vector<gfx::Animation*>(
      {&top_bar_color_animation_, &move_camera_button_to_left_animation_,
       &show_back_to_tab_button_animation_, &show_close_button_animation_});
}

std::vector<gfx::Animation*>
PictureInPictureBrowserFrameView::GetRenderInactiveAnimationsForTesting() {
  return std::vector<gfx::Animation*>(
      {&top_bar_color_animation_, &move_camera_button_to_right_animation_,
       &hide_back_to_tab_button_animation_, &hide_close_button_animation_});
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

void PictureInPictureBrowserFrameView::OnMouseEnteredOrExitedWindow(
    bool entered) {
  mouse_inside_window_ = entered;
  UpdateTopBarView(mouse_inside_window_);
}

BEGIN_METADATA(PictureInPictureBrowserFrameView, BrowserNonClientFrameView)
END_METADATA
