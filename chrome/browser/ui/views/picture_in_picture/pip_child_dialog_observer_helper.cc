// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/picture_in_picture/pip_child_dialog_observer_helper.h"

#include <cmath>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "ui/compositor/layer.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace {

// The time duration that child dialog animations will take in total.
constexpr base::TimeDelta kChildDialogAnimationDuration =
    base::Milliseconds(250);

// How long child-dialog-driven resizes are batched before being applied, so we
// don't spam resizes while a dialog (or a concurrent user resize) is settling.
constexpr base::TimeDelta kChildDialogResizeBatchDelay =
    base::Milliseconds(100);

}  // namespace

PipChildDialogObserverHelper::PipChildDialogObserverHelper(Delegate* delegate)
    : delegate_(delegate), pip_widget_(delegate->GetPipWidget()) {
  pip_widget_observation_.Observe(pip_widget_);
  // The bounds might not be set yet, depending on the platform, but that's
  // okay.  We'll get a callback later if not.  CrOS likes to set these
  // initially and not call us back unless the user resizes, so it's important
  // to grab the bounds now else we'll believe that the user's most recently
  // desired size is (0,0)-0x0.
  latest_user_desired_bounds_ = pip_widget_->GetWindowBoundsInScreen();
}

PipChildDialogObserverHelper::~PipChildDialogObserverHelper() = default;

void PipChildDialogObserverHelper::OnWidgetBoundsChanged(
    views::Widget* widget,
    const gfx::Rect& new_bounds) {
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
  // the expected size (either the child dialog forced size or the last known
  // user desired size), then track that too, but otherwise only change the
  // desired location.
  latest_user_desired_bounds_.set_origin(new_bounds.origin());

  // The expected size is the baseline we compare against to filter out 1-DIP
  // rounding noise. If the window is currently auto-resized for a dialog,
  // we expect it to maintain the forced dialog size. Otherwise, we expect
  // it to match the last known user-desired size.
  gfx::Size expected_size = (resizing_state_ == ResizingState::kSizedToChildren)
                                ? latest_child_dialog_forced_bounds_.size()
                                : latest_user_desired_bounds_.size();

  // Ignore 1-DIP rounding fluctuations. These can occur due to rounding noise
  // during coordinate conversion (e.g. when the window is moved to a new
  // position or crosses monitors with different scale factors). Ignoring them
  // prevents silent cache pollution, which would otherwise cause the window to
  // grow (drift) when it is closed and reopened.
  if (std::abs(new_bounds.width() - expected_size.width()) > 1 ||
      std::abs(new_bounds.height() - expected_size.height()) > 1) {
    latest_user_desired_bounds_.set_size(new_bounds.size());

    AnimateDialogsWaitingForResize();
    resizing_state_ = ResizingState::kNotSizedToChildren;
    resize_timer_.Stop();
  }

  // Notify the delegate of the updated user-desired bounds. We do this after
  // applying the 1-DIP filter to ensure the cache is updated with clean bounds.
  delegate_->OnUserDesiredBoundsChanged(latest_user_desired_bounds_);
}

void PipChildDialogObserverHelper::OnWidgetDestroying(views::Widget* widget) {
  if (widget == pip_widget_) {
    // Owners destroy this helper before the PiP widget it observes (the frame
    // view resets it in its own OnWidgetDestroying; the standalone host resets
    // it in ClosePipWindow() before tearing the widget down), so this branch is
    // only reached for the PiP widget itself and there is nothing to unwind.
    return;
  }

  CleanUpChildDialogAndMaybeRevert(widget);
}

void PipChildDialogObserverHelper::OnWidgetVisibilityChanged(
    views::Widget* widget,
    bool visible) {
  if (widget == pip_widget_) {
    return;
  }

  if (visible) {
    invisible_child_dialogs_.erase(widget);
    MaybeResizeForChildDialog(widget, /*resize_immediately=*/true);
  } else {
    invisible_child_dialogs_.insert(widget);
    MaybeRevertSizeAfterChildDialogCloses();
  }
}

void PipChildDialogObserverHelper::OnWidgetChildAdded(
    views::Widget* widget,
    views::Widget* child_dialog) {
  if (widget != pip_widget_) {
    return;
  }

  child_dialog_observations_.AddObservation(child_dialog);
  if (child_dialog->IsVisible()) {
    MaybeResizeForChildDialog(child_dialog, /*resize_immediately=*/true);
  } else {
    invisible_child_dialogs_.insert(child_dialog);
  }
}

void PipChildDialogObserverHelper::OnWidgetChildRemoved(
    views::Widget* widget,
    views::Widget* child_dialog) {
  if (widget != pip_widget_) {
    return;
  }
  // Once it's not a child widget, stop following it.
  CleanUpChildDialogAndMaybeRevert(child_dialog);
}

void PipChildDialogObserverHelper::CleanUpChildDialog(
    views::Widget* child_dialog) {
  invisible_child_dialogs_.erase(child_dialog);
  // During widget destruction, it is possible for the removal to be requested
  // multiple times (e.g., once by parent notification and once by
  // self-notification). This check ensures RemoveObservation is only called
  // once.
  if (child_dialog_observations_.IsObservingSource(child_dialog)) {
    child_dialog_observations_.RemoveObservation(child_dialog);
  }
  child_dialogs_waiting_for_resize_.erase(child_dialog);
  child_dialog_sizes_.erase(child_dialog);
}

void PipChildDialogObserverHelper::CleanUpChildDialogAndMaybeRevert(
    views::Widget* child_dialog) {
  // Bubble dialogs (e.g. Page Info, content-setting bubbles) can be anchored to
  // a view inside the PiP frame, and this callback runs while the bubble's
  // native window is already being destroyed (Widget::HandleWidgetDestroying()
  // removes the child from its parent mid-teardown). Reverting synchronously
  // would relayout the frame and re-anchor the removed bubble, which then tries
  // to set bounds on its already-destroying native window. So for bubbles we
  // post the revert to run after native destruction finishes; every other
  // dialog type is safe to revert synchronously.
  views::WidgetDelegate* const widget_delegate =
      child_dialog->widget_delegate();
  const bool defer_revert =
      widget_delegate && widget_delegate->AsBubbleDialogDelegate();
  CleanUpChildDialog(child_dialog);

  if (!defer_revert) {
    MaybeRevertSizeAfterChildDialogCloses();
    return;
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &PipChildDialogObserverHelper::MaybeRevertSizeAfterChildDialogCloses,
          weak_factory_.GetWeakPtr()));
}

void PipChildDialogObserverHelper::AnimateDialogsWaitingForResize() {
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

void PipChildDialogObserverHelper::PostResizeForChild(
    const gfx::Rect& new_bounds) {
  resizing_state_ = ResizingState::kPendingResizeForChild;
  pending_bounds_ = new_bounds;

  // If the timer is already running, then this will reset it.  That's okay; we
  // really don't want to keep spamming resizes while a user resize is in
  // progress already.
  //
  // Unretained is safe because this will cancel if it's destructed.
  resize_timer_.Start(
      FROM_HERE, kChildDialogResizeBatchDelay,
      base::BindOnce(&PipChildDialogObserverHelper::FinishPendingResizeForChild,
                     base::Unretained(this)));
}

void PipChildDialogObserverHelper::FinishPendingResizeForChild() {
  // When the timer is set, the state should be set to `kPendingResizeForChild`.
  // If anything changes the state away from `kPendingResizeForChild`, then it
  // also should cancel the timer.
  CHECK_EQ(resizing_state_, ResizingState::kPendingResizeForChild);

  resizing_state_ = ResizingState::kResizeForChildInProgress;
  pip_widget_->SetBoundsConstrained(pending_bounds_);
  // Give owners that position their own dialogs (e.g. the standalone host) a
  // chance to keep each visible dialog aligned with the resized window. The
  // Browser-backed frame view relies on its modal dialog host and uses the
  // default no-op.
  for (views::Widget* child_dialog : child_dialog_observations_.sources()) {
    if (!invisible_child_dialogs_.contains(child_dialog)) {
      delegate_->PositionChildDialog(child_dialog);
    }
  }
  delegate_->EnforceTucking();
  AnimateDialogsWaitingForResize();
  resizing_state_ = ResizingState::kSizedToChildren;
}

void PipChildDialogObserverHelper::MaybeResizeForChildDialog(
    views::Widget* child_dialog,
    bool resize_immediately) {
  // If the child dialog is not visible, do not resize the PiP window to fit it.
  if (!child_dialog->IsVisible()) {
    return;
  }

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
    required_size += delegate_->ComputeDialogPadding();

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
    // No resize is needed, but the owner may still want to reposition the
    // dialog within the unchanged window (no-op for the Browser frame view).
    delegate_->PositionChildDialog(child_dialog);
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
  if (resize_immediately) {
    RunPendingChildResize();
  }
}

void PipChildDialogObserverHelper::MaybeRevertSizeAfterChildDialogCloses() {
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
  delegate_->EnforceTucking();
}
