// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/picture_in_picture_browser_frame_view.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_occlusion_tracker.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/ui/browser_content_setting_bubble_model_delegate.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/content_settings/content_setting_image_model_states.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/frame/browser_frame_bounds_change_animation.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/overlay/overlay_window_image_button.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ui/frame/frame_utils.h"
#include "components/omnibox/browser/location_bar_model_impl.h"
#include "components/vector_icons/vector_icons.h"
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
#include "ui/gfx/animation/animation_container.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/compositor_animation_runner.h"
#include "ui/views/event_monitor.h"
#include "ui/views/layout/animating_layout_manager.h"
#include "ui/views/layout/flex_layout_view.h"
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

#if BUILDFLAG(IS_LINUX)
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/views/frame/browser_frame_view_paint_utils_linux.h"
#include "chrome/browser/ui/views/frame/desktop_browser_frame_aura_linux.h"
#include "ui/linux/linux_ui.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/base/window_state_type.h"
#include "chromeos/ui/frame/interior_resize_handler_targeter.h"
#endif

#if RESIZE_DOCUMENT_PICTURE_IN_PICTURE_TO_DIALOG
#include "ui/aura/client/transient_window_client.h"
#include "ui/aura/window.h"
#endif  // RESIZE_DOCUMENT_PICTURE_IN_PICTURE_TO_DIALOG

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/constants.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

namespace {

constexpr int kWindowIconImageSize = 16;
constexpr int kBackToTabImageSize = 16;
constexpr int kContentSettingIconSize = 16;

// The height of the controls bar at the top of the window.
constexpr int kTopControlsHeight = 34;

#if BUILDFLAG(IS_LINUX)
// Frame border when window shadow is not drawn.
constexpr int kFrameBorderThickness = 4;
#endif

#if !BUILDFLAG(IS_CHROMEOS_ASH)
constexpr int kResizeBorder = 10;
#endif
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

constexpr base::TimeDelta kShowHideAllButtonsAnimationDuration =
    kAnimationDuration;

class BackToTabButton : public OverlayWindowImageButton {
  METADATA_HEADER(BackToTabButton, OverlayWindowImageButton)

 public:
  explicit BackToTabButton(PressedCallback callback)
      : OverlayWindowImageButton(std::move(callback)) {
    auto* icon = &vector_icons::kBackToTabChromeRefreshIcon;
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

    gfx::Rect input_bounds = pip_browser_frame_view_->GetLocalBounds();

#if BUILDFLAG(IS_LINUX)
    // Calculate input bounds for Linux. This is needed because the input bounds
    // is not necessary the same as the local bounds on Linux.
    if (pip_browser_frame_view_->ShouldDrawFrameShadow()) {
      gfx::Insets insets =
          pip_browser_frame_view_->RestoredMirroredFrameBorderInsets();
      if (pip_browser_frame_view_->frame()->tiled()) {
        insets = gfx::Insets();
      }
      input_bounds.Inset(insets - pip_browser_frame_view_->GetInputInsets());
    }
#endif

    return input_bounds.Contains(point);
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
    frame_view.browser_view()->Close();
  }
}

}  // namespace

#if RESIZE_DOCUMENT_PICTURE_IN_PICTURE_TO_DIALOG
PictureInPictureBrowserFrameView::ChildDialogObserverHelper::
    ChildDialogObserverHelper(views::Widget* pip_widget)
    : pip_widget_(pip_widget) {
  pip_widget_observation_.Observe(pip_widget_);
  aura_window_observation_.Observe(pip_widget_->GetNativeWindow());
  transient_window_observation_.Observe(
      aura::client::GetTransientWindowClient());
}

PictureInPictureBrowserFrameView::ChildDialogObserverHelper::
    ~ChildDialogObserverHelper() = default;

void PictureInPictureBrowserFrameView::ChildDialogObserverHelper::
    OnWidgetBoundsChanged(views::Widget* widget, const gfx::Rect& new_bounds) {
  if (widget != pip_widget_) {
    return;
  }

  // If this bounds change is due to a dialog opening, then track that adjusted
  // bounds.
  if (resizing_state_ == ResizingState::kDuringInitialResizeForNewChild) {
    latest_child_dialog_forced_bounds_ = new_bounds;
    return;
  }

  // Otherwise, this was due to a user resizing/moving the window, so track this
  // new location as a user-desired one. If they've also changed the size from
  // the child-dialog-forced size, then track that too, but otherwise only
  // change the desired location.
  latest_user_desired_bounds_.set_origin(new_bounds.origin());
  if (resizing_state_ == ResizingState::kNormal ||
      new_bounds.size() != latest_child_dialog_forced_bounds_.size()) {
    latest_user_desired_bounds_.set_size(new_bounds.size());

    // At this point, we'll no longer resize when the child dialog closes, so
    // reset the state to normal.
    resizing_state_ = ResizingState::kNormal;
  }
}

void PictureInPictureBrowserFrameView::ChildDialogObserverHelper::
    OnWidgetDestroying(views::Widget* widget) {
  if (widget == pip_widget_) {
    return;
  }

  invisible_child_dialogs_.erase(widget);
  child_dialog_observations_.RemoveObservation(widget);

  MaybeRevertSizeAfterChildDialogCloses();
}

void PictureInPictureBrowserFrameView::ChildDialogObserverHelper::
    OnWidgetVisibilityChanged(views::Widget* widget, bool visible) {
  if (widget == pip_widget_) {
    return;
  }

  if (visible) {
    invisible_child_dialogs_.erase(widget);
    MaybeResizeForChildDialog(widget);
  } else {
    invisible_child_dialogs_.insert(widget);
    MaybeRevertSizeAfterChildDialogCloses();
  }
}

void PictureInPictureBrowserFrameView::ChildDialogObserverHelper::OnWindowAdded(
    aura::Window* new_window) {
  auto* child_dialog = views::Widget::GetWidgetForNativeWindow(new_window);
  if (child_dialog) {
    OnChildDialogOpened(child_dialog);
  }
}

void PictureInPictureBrowserFrameView::ChildDialogObserverHelper::
    OnTransientChildWindowAdded(aura::Window* parent,
                                aura::Window* transient_child) {
  if (parent != pip_widget_->GetNativeWindow()) {
    return;
  }

  auto* child_dialog = views::Widget::GetWidgetForNativeWindow(transient_child);
  if (child_dialog) {
    OnChildDialogOpened(child_dialog);
  }
}

void PictureInPictureBrowserFrameView::ChildDialogObserverHelper::
    OnChildDialogOpened(views::Widget* child_dialog) {
  child_dialog_observations_.AddObservation(child_dialog);
  if (child_dialog->IsVisible()) {
    MaybeResizeForChildDialog(child_dialog);
  } else {
    invisible_child_dialogs_.insert(child_dialog);
  }
}

void PictureInPictureBrowserFrameView::ChildDialogObserverHelper::
    MaybeResizeForChildDialog(views::Widget* child_dialog) {
  gfx::Rect original_bounds = pip_widget_->GetWindowBoundsInScreen();
  gfx::Rect dialog_bounds = child_dialog->GetWindowBoundsInScreen();

  gfx::Rect adjusted_bounds = original_bounds;
  adjusted_bounds.Union(dialog_bounds);

  if (adjusted_bounds == original_bounds) {
    return;
  }

  resizing_state_ = ResizingState::kDuringInitialResizeForNewChild;
  pip_widget_->SetBoundsConstrained(adjusted_bounds);
  resizing_state_ = ResizingState::kSizedToChildren;
}

void PictureInPictureBrowserFrameView::ChildDialogObserverHelper::
    MaybeRevertSizeAfterChildDialogCloses() {
  // If we still have another visible child dialog, continue to maintain the
  // size.
  if (child_dialog_observations_.GetSourcesCount() >
      invisible_child_dialogs_.size()) {
    return;
  }

  // If we no longer have any child dialogs and we had resized for one, then
  // adjust back to the user-preferred size.
  if (resizing_state_ == ResizingState::kNormal) {
    return;
  }
  resizing_state_ = ResizingState::kNormal;
  pip_widget_->SetBoundsConstrained(latest_user_desired_bounds_);
}
#endif  // RESIZE_DOCUMENT_PICTURE_IN_PICTURE_TO_DIALOG

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
                                    0.0)}),
      show_all_buttons_animation_(kShowHideAllButtonsAnimationDuration,
                                  gfx::LinearAnimation::kDefaultFrameRate,
                                  this),
      hide_all_buttons_animation_(kShowHideAllButtonsAnimationDuration,
                                  gfx::LinearAnimation::kDefaultFrameRate,
                                  this) {
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

  // Creates the window icon.
  const gfx::FontList& font_list = views::TypographyProvider::Get().GetFont(
      CONTEXT_OMNIBOX_PRIMARY, views::style::STYLE_PRIMARY);
  location_icon_view_ = top_bar_container_view_->AddChildView(
      std::make_unique<LocationIconView>(font_list, this, this));
    // The PageInfo icon should be 8px from the left of the window and 4px from
    // the right of the origin.
    location_icon_view_->SetProperty(views::kMarginsKey,
                                     gfx::Insets::TLBR(0, 8, 0, 4));

  // For file URLs, we want to elide the tail, since the file name and/or query
  // part of the file URL can be made to look like an origin for spoofing. For
  // HTTPS URLs, we elide the head to prevent spoofing via long origins, since
  // in the HTTPS case everything besides the origin is removed for display.
  auto elide_behavior = location_bar_model_->GetURL().SchemeIsFile()
                            ? gfx::ELIDE_TAIL
                            : gfx::ELIDE_HEAD;

  // Similarly for extension URLs, the tail is more important to elide.
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (location_bar_model_->GetURL().SchemeIs(extensions::kExtensionScheme)) {
    elide_behavior = gfx::ELIDE_TAIL;
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  // Creates the window title.
  top_bar_container_view_->AddChildView(
      views::Builder<views::Label>()
          .CopyAddressTo(&window_title_)
          .SetText(location_bar_model_->GetURLForDisplay())
          .SetHorizontalAlignment(gfx::ALIGN_LEFT)
          .SetElideBehavior(elide_behavior)
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
    model->SetIconSize(kContentSettingIconSize);
    auto image_view = std::make_unique<ContentSettingImageView>(
        std::move(model), this, this, font_list);

    // The ContentSettingImageView loses 4px of margin that we don't want to
    // lose in the document picture-in-picture toolbar.
    image_view->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(0, 0, 0, 4));

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

  // Enable button layer rendering to set opacity for animation.
  if (back_to_tab_button_) {
    back_to_tab_button_->SetPaintToLayer();
    back_to_tab_button_->layer()->SetFillsBoundsOpaquely(false);
  }
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
  if (back_to_tab_button_) {
    show_back_to_tab_button_animation_.set_continuous(false);
    show_back_to_tab_button_animation_.set_delegate(this);
    hide_back_to_tab_button_animation_.set_continuous(false);
    hide_back_to_tab_button_animation_.set_delegate(this);
  }
  show_close_button_animation_.set_continuous(false);
  show_close_button_animation_.set_delegate(this);
  hide_close_button_animation_.set_continuous(false);
  hide_close_button_animation_.set_delegate(this);

  // If the window manager wants us to display an overlay, get it.  In practice,
  // this is the auto-pip Allow / Block content setting UI.
  if (auto auto_pip_setting_overlay =
          PictureInPictureWindowManager::GetInstance()->GetOverlayView(
              top_bar_container_view_, views::BubbleBorder::TOP_CENTER)) {
    auto_pip_setting_overlay_ =
        AddChildView(std::move(auto_pip_setting_overlay));
  }

#if BUILDFLAG(IS_LINUX)
  auto* profile = browser_view->browser()->profile();
  auto* linux_ui_theme = ui::LinuxUiTheme::GetForProfile(profile);
  auto* theme_service_factory = ThemeServiceFactory::GetForProfile(profile);
  if (linux_ui_theme && theme_service_factory->UsingSystemTheme()) {
    bool solid_frame = !static_cast<DesktopBrowserFrameAuraLinux*>(
                            frame->native_browser_frame())
                            ->ShouldDrawRestoredFrameShadow();

    // This may return null, but that's handled below.
    window_frame_provider_ =
        linux_ui_theme->GetWindowFrameProvider(solid_frame, /*tiled=*/false);
  }

  // Only one of window_frame_provider_ and frame_background_ will be used.
  if (!window_frame_provider_) {
    frame_background_ = std::make_unique<views::FrameBackground>();
  }
#endif


#if BUILDFLAG(IS_CHROMEOS_LACROS)
  frame->GetNativeWindow()->SetEventTargeter(
      std::make_unique<chromeos::InteriorResizeHandleTargeter>(
          base::BindRepeating([](const aura::Window* window) {
            return window->GetProperty(chromeos::kWindowStateTypeKey);
          })));
#endif
}

PictureInPictureBrowserFrameView::~PictureInPictureBrowserFrameView() {
  base::UmaHistogramEnumeration("Media.DocumentPictureInPicture.CloseReason",
                                close_reason_);
}

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

void PictureInPictureBrowserFrameView::OnBrowserViewInitViewsComplete() {
  BrowserNonClientFrameView::OnBrowserViewInitViewsComplete();

#if BUILDFLAG(IS_WIN)
  const gfx::Insets insets = GetClientAreaInsets(
      MonitorFromWindow(HWNDForView(this), MONITOR_DEFAULTTONEAREST));
#else
  const gfx::Insets insets;
#endif

  const std::optional<blink::mojom::PictureInPictureWindowOptions> pip_options =
      browser_view()->GetDocumentPictureInPictureOptions();

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

  // Get the current display. This is needed by |ComputeOuterWindowBounds| to
  // determine the work area dimensions and the allowed maximum window size.
  const BrowserWindow* const browser_window =
      browser_view()->browser()->window();
  const gfx::NativeWindow native_window =
      browser_window ? browser_window->GetNativeWindow() : gfx::NativeWindow();
  const display::Screen* const screen = display::Screen::GetScreen();
  const gfx::Rect original_override_bounds =
      browser_view()->browser()->override_bounds();
  display::Display display;
  // Use the override bounds if possible, since the NativeWindow might not be
  // positioned properly yet.
  if (!original_override_bounds.IsEmpty()) {
    display =
        screen->GetDisplayNearestPoint(original_override_bounds.top_center());
  } else {
    display = browser_window ? screen->GetDisplayNearestWindow(native_window)
                             : screen->GetDisplayForNewWindows();
  }

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
          pip_options.value(), display,
          GetMinimumSize() + gfx::Size(insets.width(), insets.height()),
          excluded_margin);

  browser_view()->browser()->set_override_bounds(window_bounds);
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
  if (!GetWidget() || !GetWidget()->GetNativeWindow()) {
    // The maximum size can't be smaller than the minimum size.
    return GetMinimumSize();
  }

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

  LayoutSuperclass<BrowserNonClientFrameView>(this);
}

void PictureInPictureBrowserFrameView::AddedToWidget() {
  widget_observation_.Observe(GetWidget());
  window_event_observer_ = std::make_unique<WindowEventObserver>(this);
#if RESIZE_DOCUMENT_PICTURE_IN_PICTURE_TO_DIALOG
  child_dialog_observer_helper_ =
      std::make_unique<ChildDialogObserverHelper>(GetWidget());
#endif  // RESIZE_DOCUMENT_PICTURE_IN_PICTURE_TO_DIALOG

  // Creates an animation container to ensure all the animations update at the
  // same time.
  gfx::AnimationContainer* animation_container = new gfx::AnimationContainer();
  animation_container->SetAnimationRunner(
      std::make_unique<views::CompositorAnimationRunner>(GetWidget()));
  top_bar_color_animation_.SetContainer(animation_container);
  move_camera_button_to_left_animation_.SetContainer(animation_container);
  move_camera_button_to_right_animation_.SetContainer(animation_container);
  show_all_buttons_animation_.SetContainer(animation_container);
  hide_all_buttons_animation_.SetContainer(animation_container);

  if (back_to_tab_button_) {
    show_back_to_tab_button_animation_.SetContainer(animation_container);
    hide_back_to_tab_button_animation_.SetContainer(animation_container);
    show_close_button_animation_.SetContainer(animation_container);
    hide_close_button_animation_.SetContainer(animation_container);
  }

  // TODO(crbug.com/40279642): Don't force dark mode once we support a
  // light mode window.
  GetWidget()->SetColorModeOverride(ui::ColorProviderKey::ColorMode::kDark);

  // If the AutoPiP setting overlay is set, show the permission settings bubble.
  if (auto_pip_setting_overlay_) {
    auto_pip_setting_overlay_->ShowBubble(GetWidget()->GetNativeView());
  }

  PictureInPictureOcclusionTracker* tracker =
      PictureInPictureWindowManager::GetInstance()->GetOcclusionTracker();
  if (tracker) {
    tracker->OnPictureInPictureWidgetOpened(GetWidget());
  }

  BrowserNonClientFrameView::AddedToWidget();
}

void PictureInPictureBrowserFrameView::RemovedFromWidget() {
  widget_observation_.Reset();
  window_event_observer_.reset();
#if RESIZE_DOCUMENT_PICTURE_IN_PICTURE_TO_DIALOG
  child_dialog_observer_helper_.reset();
#endif  // RESIZE_DOCUMENT_PICTURE_IN_PICTURE_TO_DIALOG

  // Clear the AutoPiP setting overlay view.
  if (auto_pip_setting_overlay_) {
    auto_pip_setting_overlay_ = nullptr;
  }

  BrowserNonClientFrameView::RemovedFromWidget();
}

#if BUILDFLAG(IS_LINUX)
gfx::Insets
PictureInPictureBrowserFrameView::RestoredMirroredFrameBorderInsets() const {
  auto border = FrameBorderInsets();
  return base::i18n::IsRTL() ? gfx::Insets::TLBR(border.top(), border.right(),
                                                 border.bottom(), border.left())
                             : border;
}

gfx::Insets PictureInPictureBrowserFrameView::GetInputInsets() const {
  return gfx::Insets(ShouldDrawFrameShadow() ? kResizeBorder : 0);
}

SkRRect PictureInPictureBrowserFrameView::GetRestoredClipRegion() const {
  gfx::RectF bounds_dip(GetLocalBounds());
  if (ShouldDrawFrameShadow()) {
    gfx::InsetsF border(RestoredMirroredFrameBorderInsets());
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

void PictureInPictureBrowserFrameView::SetFrameBounds(const gfx::Rect& bounds) {
  if (!base::FeatureList::IsEnabled(
          media::kDocumentPictureInPictureAnimateResize) ||
      !gfx::Animation::ShouldRenderRichAnimation()) {
    BrowserNonClientFrameView::SetFrameBounds(bounds);
    return;
  }
  bounds_change_animation_ =
      std::make_unique<BrowserFrameBoundsChangeAnimation>(*frame(), bounds);
  bounds_change_animation_->Start();
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
  if (!contents)
    return false;

  views::BubbleDialogDelegateView* bubble =
      PageInfoBubbleView::CreatePageInfoBubble(
          location_icon_view_, gfx::Rect(), GetWidget()->GetNativeWindow(),
          contents, contents->GetLastCommittedURL(),
          /*initialized_callback=*/base::DoNothing(),
          /*closing_callback=*/base::DoNothing(),
          /*allow_about_this_site=*/false);
  bubble->SetHighlightedButton(location_icon_view_);
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
    LocationIconView::Delegate::IconFetchedCallback on_icon_fetched) const {
    // If we're animating between colors, use the current color value.
    if (current_foreground_color_.has_value()) {
      return ui::ImageModel::FromVectorIcon(
          location_bar_model_->GetVectorIcon(), *current_foreground_color_,
          kWindowIconImageSize);
    }

    ui::ColorId foreground_color_id =
        (top_bar_color_animation_.GetCurrentValue() == 0)
            ? kColorPipWindowForegroundInactive
            : kColorPipWindowForeground;

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
  Browser* browser = chrome::FindBrowserWithTab(GetWebContents());
  return browser->content_setting_bubble_model_delegate();
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
#if RESIZE_DOCUMENT_PICTURE_IN_PICTURE_TO_DIALOG
  child_dialog_observer_helper_.reset();
#endif  // RESIZE_DOCUMENT_PICTURE_IN_PICTURE_TO_DIALOG
}

void PictureInPictureBrowserFrameView::OnWidgetBoundsChanged(
    views::Widget* widget,
    const gfx::Rect& new_bounds) {
  PictureInPictureWindowManager::GetInstance()->UpdateCachedBounds(new_bounds);
}

///////////////////////////////////////////////////////////////////////////////
// gfx::AnimationDelegate implementations:

void PictureInPictureBrowserFrameView::AnimationEnded(
    const gfx::Animation* animation) {
  if (animation == &top_bar_color_animation_) {
    current_foreground_color_ = std::nullopt;
    location_icon_view_->Update(/*suppress_animations=*/false);
  }
}

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
      current_foreground_color_ = color;
      location_icon_view_->Update(/*suppress_animations=*/false);
    return;
  }

  if (animation == &move_camera_button_to_left_animation_ ||
      animation == &move_camera_button_to_right_animation_) {
    int close_and_back_to_tab_button_combined_widths =
        close_image_button_->width();
    if (back_to_tab_button_) {
      close_and_back_to_tab_button_combined_widths +=
          back_to_tab_button_->width();
    }
    for (ContentSettingImageView* view : content_setting_views_) {
      // Set the position of camera icon relative to |button_container_view_|.
      view->SetX(animation->CurrentValueBetween(
          close_and_back_to_tab_button_combined_widths, 0));
    }
    return;
  }

  if (animation == &show_all_buttons_animation_ ||
      animation == &hide_all_buttons_animation_) {
    double animation_current_value = animation->GetCurrentValue();

    // Update the animation current value when running "hide" annimations. Since
    // `hide_all_buttons_animation_` uses `gfx::LinearAnimation`, which goes
    // from 0.0 to 1.0.
    if (animation == &hide_all_buttons_animation_) {
      animation_current_value = 1.0 - animation_current_value;
    }
    if (back_to_tab_button_) {
      back_to_tab_button_->layer()->SetOpacity(animation_current_value);
    }
    close_image_button_->layer()->SetOpacity(animation_current_value);
    return;
  }

  // If there are no visible content setting views, return, since show/hide all
  // buttons animation has already taken care of animating all buttons.
  if (!HasAnyVisibleContentSettingViews()) {
    return;
  }

  if (animation == &show_back_to_tab_button_animation_ ||
      animation == &hide_back_to_tab_button_animation_) {
    CHECK(back_to_tab_button_);
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
        GetInputInsets());
  } else {
    DCHECK(frame_background_);
    frame_background_->set_frame_color(
        GetColorProvider()->GetColor(kColorPipWindowTopBarBackground));
    frame_background_->set_use_custom_frame(frame()->UseCustomFrame());
    frame_background_->set_is_active(ShouldPaintAsActive());
    frame_background_->set_theme_image(GetFrameImage());

    frame_background_->set_theme_image_inset(
        browser_view()->GetThemeOffsetFromBrowserView());
    frame_background_->set_theme_overlay_image(GetFrameOverlayImage());
    frame_background_->set_top_area_height(GetTopAreaHeight());
    PaintRestoredFrameBorderLinux(
        *canvas, *this, frame_background_.get(), GetRestoredClipRegion(),
        ShouldDrawFrameShadow(), ShouldPaintAsActive(),
        RestoredMirroredFrameBorderInsets(), GetShadowValues(),
        frame()->tiled());
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
  const auto kButtonContainerViewWithCameraButtonInsets =
      gfx::Insets::TLBR(0, 0, 0, GetLayoutConstant(TAB_AFTER_TITLE_PADDING));
  const auto kButtonContainerViewInsets =
      gfx::Insets::VH(0, GetLayoutConstant(TAB_AFTER_TITLE_PADDING));

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
  // Check if the update is needed to avoid redundant animations.
  if (render_active_ == render_active) {
    return;
  }

  render_active_ = render_active;

  bool has_any_visible_content_setting_views =
      HasAnyVisibleContentSettingViews();

  // Stop the previous animations since if this function is called too soon,
  // previous animations may override the new animations.
  if (render_active_) {
    move_camera_button_to_right_animation_.Stop();
    if (has_any_visible_content_setting_views) {
      if (back_to_tab_button_) {
        hide_back_to_tab_button_animation_.Stop();
      }
      hide_close_button_animation_.Stop();
    } else {
      hide_all_buttons_animation_.Stop();
    }

    top_bar_color_animation_.Show();

    // SlideAnimation needs to be reset if only Show() is called.
    move_camera_button_to_left_animation_.Reset(0.0);
    move_camera_button_to_left_animation_.Show();

    if (has_any_visible_content_setting_views) {
      if (back_to_tab_button_) {
        show_back_to_tab_button_animation_.Start();
      }
      show_close_button_animation_.Start();
    } else {
      show_all_buttons_animation_.Start();
    }
  } else {
    move_camera_button_to_left_animation_.Stop();

    if (has_any_visible_content_setting_views) {
      if (back_to_tab_button_) {
        show_back_to_tab_button_animation_.Stop();
      }
      show_close_button_animation_.Stop();
    } else {
      show_all_buttons_animation_.Stop();
    }

    top_bar_color_animation_.Hide();
    move_camera_button_to_right_animation_.Start();
    if (has_any_visible_content_setting_views) {
      if (back_to_tab_button_) {
        hide_back_to_tab_button_animation_.Start();
      }
      hide_close_button_animation_.Start();
    } else {
      hide_all_buttons_animation_.Start();
    }
  }
}

gfx::Insets PictureInPictureBrowserFrameView::FrameBorderInsets() const {
#if BUILDFLAG(IS_LINUX)
  if (window_frame_provider_) {
    const auto insets = window_frame_provider_->GetFrameThicknessDip();
    const bool tiled = frame()->tiled();

    // If edges of the window are tiled and snapped to the edges of the desktop,
    // window_frame_provider_ will skip drawing.
    return tiled ? gfx::Insets() : insets;
  }
  return GetRestoredFrameBorderInsetsLinux(ShouldDrawFrameShadow(),
                                           gfx::Insets(kFrameBorderThickness),
                                           GetShadowValues(), kResizeBorder);
#else
  return gfx::Insets();
#endif
}

gfx::Insets PictureInPictureBrowserFrameView::ResizeBorderInsets() const {
#if BUILDFLAG(IS_LINUX)
  return FrameBorderInsets();
#elif !BUILDFLAG(IS_CHROMEOS_ASH)
  return gfx::Insets(kResizeBorder);
#else
  return gfx::Insets();
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

#if BUILDFLAG(IS_WIN)
gfx::Insets PictureInPictureBrowserFrameView::GetClientAreaInsets(
    HMONITOR monitor) const {
  const int frame_thickness = ui::GetFrameThickness(monitor);
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
std::vector<gfx::Animation*>
PictureInPictureBrowserFrameView::GetRenderActiveAnimationsForTesting() {
  DCHECK(render_active_);
  std::vector<gfx::Animation*> animations(
      {&top_bar_color_animation_, &move_camera_button_to_left_animation_,
       &show_close_button_animation_});
  if (back_to_tab_button_) {
    animations.push_back(&show_back_to_tab_button_animation_);
  }
  if (!HasAnyVisibleContentSettingViews()) {
    animations.push_back(&show_all_buttons_animation_);
  }
  return animations;
}

std::vector<gfx::Animation*>
PictureInPictureBrowserFrameView::GetRenderInactiveAnimationsForTesting() {
  DCHECK(!render_active_);
  std::vector<gfx::Animation*> animations(
      {&top_bar_color_animation_, &move_camera_button_to_right_animation_,
       &hide_close_button_animation_});
  if (back_to_tab_button_) {
    animations.push_back(&hide_back_to_tab_button_animation_);
  }
  if (!HasAnyVisibleContentSettingViews()) {
    animations.push_back(&hide_all_buttons_animation_);
  }
  return animations;
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

BEGIN_METADATA(PictureInPictureBrowserFrameView)
END_METADATA
