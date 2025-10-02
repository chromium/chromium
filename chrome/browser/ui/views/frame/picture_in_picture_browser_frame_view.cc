// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/picture_in_picture_browser_frame_view.h"

#include <algorithm>
#include <memory>

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_occlusion_tracker.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/ui/browser_content_setting_bubble_model_delegate.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/content_settings/content_setting_image_model_states.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/overlay/overlay_window_image_button.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_specification.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view.h"
#include "chrome/browser/ui/views/picture_in_picture/picture_in_picture_bounds_change_animation.h"
#include "chrome/browser/ui/views/picture_in_picture/picture_in_picture_tucker.h"
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
#include "ui/gfx/animation/animation_container.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/animation_builder.h"
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

// The time duration that the top bar animation will take in total.
constexpr base::TimeDelta kAnimationDuration = base::Milliseconds(250);

// The time duration that child dialog animations will take in total.
constexpr base::TimeDelta kChildDialogAnimationDuration =
    base::Milliseconds(250);

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
    frame_view.browser_view()->Close();
  }
}

}  // namespace

PictureInPictureBrowserFrameView::ChildDialogObserverHelper::
    ChildDialogObserverHelper(PictureInPictureBrowserFrameView* pip_frame,
                              BrowserView* browser_view)
    : pip_frame_(pip_frame), pip_widget_(pip_frame->GetWidget()) {
  pip_widget_observation_.Observe(pip_widget_);
  // The bounds might not be set yet, depending on the platform, but that's
  // okay.  We'll get a callback later if not.  CrOS likes to set these
  // initially and not call us back unless the user resizes, so it's important
  // to grab the bounds now else we'll believe that the user's most recently
  // desired size is (0,0)-0x0.
  latest_user_desired_bounds_ = pip_widget_->GetWindowBoundsInScreen();
}

PictureInPictureBrowserFrameView::ChildDialogObserverHelper::
    ~ChildDialogObserverHelper() = default;

void PictureInPictureBrowserFrameView::ChildDialogObserverHelper::
    OnWidgetBoundsChanged(views::Widget* widget, const gfx::Rect& new_bounds) {
  if (widget != pip_widget_) {
    // If a child is resizing, then make sure that we still contain it.  Some
    // dialogs (e.g., the camera dialog) actually do this.  Remember that we
    // won't shrink the pip window as a result of this, so it should reach
    // steady-state at some point even if it's the maximum size of the window.
    MaybeResizeForChildDialog(widget);
    return;
  }

  // If this bounds change is due to a dialog opening, then track that adjusted
  // bounds.
  if (resizing_state_ == ResizingState::kResizeForChildInProgress) {
    latest_child_dialog_forced_bounds_ = new_bounds;
    return;
  }

  // Otherwise, this was due to a user resizing/moving the window, so track this
  // new location as a user-desired one. If they've also changed the size from
  // the child-dialog-forced size, then track that too, but otherwise only
  // change the desired location.
  latest_user_desired_bounds_.set_origin(new_bounds.origin());
  if (resizing_state_ != ResizingState::kSizedToChildren ||
      new_bounds.size() != latest_child_dialog_forced_bounds_.size()) {
    latest_user_desired_bounds_.set_size(new_bounds.size());

    // At this point, we'll no longer resize when the child dialog closes, so
    // reset the state to normal.
    AnimateDialogsWaitingForResize();
    resizing_state_ = ResizingState::kNotSizedToChildren;
    resize_timer_.Stop();
  }
}

void PictureInPictureBrowserFrameView::ChildDialogObserverHelper::
    OnWidgetDestroying(views::Widget* widget) {
  if (widget == pip_widget_) {
    return;
  }

  invisible_child_dialogs_.erase(widget);
  child_dialog_observations_.RemoveObservation(widget);
  child_dialogs_waiting_for_resize_.erase(widget);
  child_dialog_sizes_.erase(widget);

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

void PictureInPictureBrowserFrameView::ChildDialogObserverHelper::
    OnWidgetChildAdded(views::Widget* widget, views::Widget* child_dialog) {
  if (widget != pip_widget_) {
    return;
  }

  child_dialog_observations_.AddObservation(child_dialog);
  if (child_dialog->IsVisible()) {
    MaybeResizeForChildDialog(child_dialog);
  } else {
    invisible_child_dialogs_.insert(child_dialog);
  }
}

void PictureInPictureBrowserFrameView::ChildDialogObserverHelper::
    OnWidgetChildRemoved(views::Widget* widget, views::Widget* child_dialog) {
  if (widget != pip_widget_) {
    return;
  }
  // Once it's not a child widget, stop following it.
  OnWidgetDestroying(child_dialog);
}

void PictureInPictureBrowserFrameView::ChildDialogObserverHelper::
    AnimateDialogsWaitingForResize() {
  if (child_dialogs_waiting_for_resize_.empty()) {
    return;
  }

  for (auto child_dialog : child_dialogs_waiting_for_resize_) {
    // If the dialog is already visible, don't re-animate it.
    if (child_dialog->GetLayer()->GetTargetOpacity() == 1.0f) {
      continue;
    }

    // Enable visibility changed animations after resizing the
    // picture-in-picture window.
    child_dialog->SetVisibilityChangedAnimationsEnabled(true);
    // Fade-in the child dialog now that the picture-in-picture window is the
    // correct size.
    views::AnimationBuilder()
        .SetPreemptionStrategy(ui::LayerAnimator::REPLACE_QUEUED_ANIMATIONS)
        .Once()
        .SetDuration(kChildDialogAnimationDuration)
        .SetOpacity(child_dialog->GetLayer(), 1.0f);
    // Allow the view to process events.
    child_dialog->GetContentsView()->SetCanProcessEventsWithinSubtree(true);
  }
}

void PictureInPictureBrowserFrameView::ChildDialogObserverHelper::
    PostResizeForChild(const gfx::Rect& new_bounds) {
  resizing_state_ = ResizingState::kPendingResizeForChild;
  pending_bounds_ = new_bounds;

  // If the timer is already running, then this will reset it.  That's okay; we
  // really don't want to keep spamming resizes while a user resize is in
  // progress already.
  //
  // Unretained is safe because this will cancel if it's destructed.
  resize_timer_.Start(
      FROM_HERE, base::Milliseconds(100),
      base::BindOnce(&PictureInPictureBrowserFrameView::
                         ChildDialogObserverHelper::FinishPendingResizeForChild,
                     base::Unretained(this)));
}

void PictureInPictureBrowserFrameView::ChildDialogObserverHelper::
    FinishPendingResizeForChild() {
  // When the timer is set, the state should be set to `kPendingResizeForChild`.
  // If anything changes the state away from `kPendingResizeForChild`, then it
  // also should cancel the timer.
  CHECK_EQ(resizing_state_, ResizingState::kPendingResizeForChild);

  resizing_state_ = ResizingState::kResizeForChildInProgress;
  pip_widget_->SetBoundsConstrained(pending_bounds_);
  pip_frame_->EnforceTucking();
  AnimateDialogsWaitingForResize();
  resizing_state_ = ResizingState::kSizedToChildren;
}

void PictureInPictureBrowserFrameView::ChildDialogObserverHelper::
    MaybeResizeForChildDialog(views::Widget* child_dialog) {
  // If the pip window in the process of closing ignore any resizes that could
  // occur as child dialogs are destroyed during teardown.
  if (pip_widget_->IsClosed()) {
    return;
  }

  if (resizing_state_ == ResizingState::kResizeForChildInProgress) {
    // If we're in the middle of a resize to match the child, ignore any
    // resizes that the child might do as a result.
    return;
  }

  // If the timer is running when a dialog opens, we use those bounds instead.
  // Note that any user resize would have cancelled the timer, so we know that
  // the pending bounds are the most recent if the timer is still running.
  const gfx::Rect original_bounds =
      resize_timer_.IsRunning() ? pending_bounds_
                                : pip_widget_->GetWindowBoundsInScreen();
  gfx::Rect dialog_bounds = child_dialog->GetWindowBoundsInScreen();
  gfx::Rect adjusted_bounds = original_bounds;

  // If the child dialog is contained within the picture-in-picture window and
  // its size has not changed, do not resize the picture-in-picture window.
  //
  // On some platforms, Mac specifically, the child widget may resize after the
  // picture-in-picture window resizes to contain the child. To avoid
  // unnecessarily re-resizing the window, we check if the child dialog is
  // contained within the picture-in-picture window and if its size is
  // unchanged, if those conditions are met then do not resize.
  auto it = child_dialog_sizes_.find(child_dialog);
  if (original_bounds.Contains(dialog_bounds) &&
      it != child_dialog_sizes_.end() && it->second == dialog_bounds.size()) {
    return;
  }

  child_dialog_sizes_.insert_or_assign(child_dialog, dialog_bounds.size());

  if (child_dialog->IsModal()) {
    // Modal dialogs will be resized / moved to use the available space, so we
    // only need to make sure that the pip window is big enough, accounting for
    // some padding that the ModalDialogHost won't allow a dialog to use.  We
    // don't care how this padding is distributed around the edge; the host will
    // move the dialog inside it.  We just care about the total amount.

    // Start with how big the dialog should be.  If it's larger than its
    // preferred size already, then keep it.  Note that the root view's minimum
    // size is usually the preferred size, while the contents view's min size
    // tends to be too small for the dialog to be useful.  This check makes sure
    // that the dialog isn't requesting anything smaller than its preferred
    // size.
    gfx::Size required_size = dialog_bounds.size();
    required_size.SetToMax(child_dialog->GetRootView()->GetMinimumSize());

    // Compute the minimum size the pip window needs to be so that it reports
    // its maximum dialog size as large enough for a dialog of size
    // `required_size`.
    required_size += pip_frame_->ComputeDialogPadding();

    // Don't shrink the window if the minimum required size is smaller.
    required_size.SetToMax(original_bounds.size());

    adjusted_bounds.set_size(required_size);
  } else if (!child_dialog->GetIsDesktopWidget()) {
    // Non-modal dialogs set their bounds directly.  If the child window is not
    // a desktop widget, then it will be clipped by the parent window.  Expand
    // the pip window to include the child dialog.
    // ChromeOS is unique in that it does not clip non-desktop widgets to the
    // parent window. So skip resizing the pip window on ChromeOS.
#if !BUILDFLAG(IS_CHROMEOS)
    adjusted_bounds.Union(dialog_bounds);
#endif
  } else {
    // Non-modal dialogs that are desktop widgets set their bounds directly and
    // are not clipped to the parent window bounds, so just leave it as is.
    return;
  }

  if (adjusted_bounds == original_bounds) {
    return;
  }

  // If the dialog is not already pending a resize, then set it up to be.
  if (!child_dialogs_waiting_for_resize_.contains(child_dialog)) {
    // Disable visibility changed animations for the child dialog. This is done
    // to prevent "flickering" due to conflicts between the picture-in-picture
    // window resize and the child dialog animation.
    child_dialog->SetVisibilityChangedAnimationsEnabled(false);
    // Don't allow the view to process events.
    child_dialog->GetContentsView()->SetCanProcessEventsWithinSubtree(false);
    child_dialog->GetLayer()->SetOpacity(0.0f);
    child_dialogs_waiting_for_resize_.insert(child_dialog);
  }

  PostResizeForChild(adjusted_bounds);
}

void PictureInPictureBrowserFrameView::ChildDialogObserverHelper::
    MaybeRevertSizeAfterChildDialogCloses() {
  // If the pip window in the process of closing ignore any resizes that could
  // occur as child dialogs are destroyed during teardown.
  if (pip_widget_->IsClosed()) {
    return;
  }

  // If we still have another visible child dialog, continue to maintain the
  // size.
  if (child_dialog_observations_.GetSourcesCount() >
      invisible_child_dialogs_.size()) {
    return;
  }

  // If we no longer have any child dialogs and we had resized for one, then
  // adjust back to the user-preferred size.
  if (resizing_state_ == ResizingState::kNotSizedToChildren) {
    return;
  }
  resizing_state_ = ResizingState::kNotSizedToChildren;
  resize_timer_.Stop();
  pip_widget_->SetBoundsConstrained(latest_user_desired_bounds_);
  pip_frame_->EnforceTucking();
}

PictureInPictureBrowserFrameView::PictureInPictureBrowserFrameView(
    BrowserWidget* widget,
    BrowserView* browser_view)
    : BrowserFrameView(widget, browser_view),
      top_bar_color_animation_(this),
      move_camera_button_to_left_animation_(this),
      move_camera_button_to_right_animation_(gfx::MultiAnimation::Parts{
          {kMoveCameraButtonToRightAnimationDurations[0],
           gfx::Tween::Type::ZERO, 1.0, 1.0},
          {kMoveCameraButtonToRightAnimationDurations[1],
           gfx::Tween::Type::EASE_OUT, 1.0, 0.0}}),
      show_back_to_tab_button_animation_(
          gfx::MultiAnimation::Parts{{kShowBackToTabButtonAnimationDurations[0],
                                      gfx::Tween::Type::ZERO, 0.0, 0.0},
                                     {kShowBackToTabButtonAnimationDurations[1],
                                      gfx::Tween::Type::LINEAR, 0.0, 1.0},
                                     {kShowBackToTabButtonAnimationDurations[2],
                                      gfx::Tween::Type::ZERO, 1.0, 1.0}}),
      hide_back_to_tab_button_animation_(
          gfx::MultiAnimation::Parts{{kHideBackToTabButtonAnimationDurations[0],
                                      gfx::Tween::Type::LINEAR, 1.0, 0.0},
                                     {kHideBackToTabButtonAnimationDurations[1],
                                      gfx::Tween::Type::ZERO, 0.0, 0.0}}),
      show_close_button_animation_(gfx::MultiAnimation::Parts{
          {kCloseButtonAnimationDurations[0], gfx::Tween::Type::ZERO, 0.0, 0.0},
          {kCloseButtonAnimationDurations[1], gfx::Tween::Type::LINEAR, 0.0,
           1.0},
          {kCloseButtonAnimationDurations[2], gfx::Tween::Type::ZERO, 1.0,
           1.0}}),
      hide_close_button_animation_(gfx::MultiAnimation::Parts{
          {kCloseButtonAnimationDurations[0], gfx::Tween::Type::ZERO, 1.0, 1.0},
          {kCloseButtonAnimationDurations[1], gfx::Tween::Type::LINEAR, 1.0,
           0.0},
          {kCloseButtonAnimationDurations[2], gfx::Tween::Type::ZERO, 0.0,
           0.0}}),
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
      views::Builder<views::Label>()
          .CopyAddressTo(&window_title_)
          .SetText(location_bar_model_->GetURLForDisplay())
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
      ContentSettingImageModel::ImageType::MEDIASTREAM};
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

gfx::Rect PictureInPictureBrowserFrameView::GetBoundsForTabStripRegion(
    const gfx::Size& tabstrip_minimum_size) const {
  return gfx::Rect();
}

gfx::Rect PictureInPictureBrowserFrameView::GetBoundsForWebAppFrameToolbar(
    const gfx::Size& toolbar_preferred_size) const {
  NOTREACHED() << "Web app toolbar should never be shown in PiP.";
}

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
      std::make_unique<ChildDialogObserverHelper>(this, browser_view());

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

// Fade in animation is disabled for Document and Video Picture-in-Picture on
// Windows. On Windows, resizable windows can not be translucent. See
// crbug.com/425711450.
#if !BUILDFLAG(IS_WIN)
  if (base::FeatureList::IsEnabled(
          media::kPictureInPictureShowWindowAnimation)) {
    if (!fade_animator_) {
      fade_animator_ = std::make_unique<PictureInPictureWidgetFadeAnimator>();
    }
    fade_animator_->AnimateShowWindow(
        GetWidget(), PictureInPictureWidgetFadeAnimator::WidgetShowType::kNone);
  }
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
          location_icon_view_, GetWidget()->GetNativeWindow(), contents,
          contents->GetLastCommittedURL())
          .HideExtendedSiteInfo()
          .Build();

  views::BubbleDialogDelegateView* const bubble =
      PageInfoBubbleView::CreatePageInfoBubble(std::move(specification));
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
    return ui::ImageModel::FromVectorIcon(location_bar_model_->GetVectorIcon(),
                                          *current_foreground_color_,
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

void PictureInPictureBrowserFrameView::OnWidgetBoundsChanged(
    views::Widget* widget,
    const gfx::Rect& new_bounds) {
  const auto pip_display = display::Screen::Get()->GetDisplayNearestWindow(
      widget->GetNativeWindow());
  PictureInPictureWindowManager::GetInstance()->UpdateCachedBounds(new_bounds,
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
  auto* host = browser_view()->GetWebContentsModalDialogHost();
  if (!host) {
    return gfx::Size();
  }

  // This is not guaranteed, but should be fairly robust if the maximum dialog
  // size computation changes.  It also prevents us from memorizing how all of
  // it works.
  return GetWidget()->GetSize() - host->GetMaximumDialogSize();
}

BEGIN_METADATA(PictureInPictureBrowserFrameView)
END_METADATA
