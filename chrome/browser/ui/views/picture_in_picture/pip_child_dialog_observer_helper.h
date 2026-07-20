// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PICTURE_IN_PICTURE_PIP_CHILD_DIALOG_OBSERVER_HELPER_H_
#define CHROME_BROWSER_UI_VIEWS_PICTURE_IN_PICTURE_PIP_CHILD_DIALOG_OBSERVER_HELPER_H_

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class Widget;
}  // namespace views

// Observes child dialogs (permission bubbles, JavaScript dialogs, etc.) that
// parent to a Picture-in-Picture widget and resizes the PiP window so those
// dialogs are not clipped by the window's typically small size, then restores
// the pre-dialog size once they close. The observation and resize policy are
// framework-only; the small amount of owner-specific behavior (which PiP widget
// to drive, how much padding the modal dialog host reserves, and re-enforcing
// tuck state after a resize) is provided through a Delegate. This lets both the
// Browser-backed PictureInPictureBrowserFrameView and the standalone
// DocumentPipHost share a single implementation of the resize policy.
//
// This helper observes the PiP widget for its whole lifetime, so it must not
// outlive its Delegate nor the PiP widget the Delegate returns.
class PipChildDialogObserverHelper : public views::WidgetObserver {
 public:
  // Implemented by the owner of the PiP window for the parts of the resize
  // policy that depend on owner-specific state the helper does not own.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Returns the PiP widget whose child dialogs are observed. Queried once,
    // at construction.
    virtual views::Widget* GetPipWidget() = 0;

    // Returns the difference between the PiP widget's outer size and the
    // maximum size it will allow a modal child dialog to occupy. The helper
    // adds this to a dialog's required size to compute how large the window
    // must grow to contain the dialog.
    virtual gfx::Size ComputeDialogPadding() const = 0;

    // Re-applies the owner's current tuck/untuck state after the helper resizes
    // the PiP widget, so a forced-tucked window stays tucked at its new bounds.
    virtual void EnforceTucking() = 0;

    // Repositions `child_dialog` when the PiP window is not resized to grow
    // around it (e.g. to keep the dialog aligned with the window's client
    // area). The Browser-backed frame view relies on its modal dialog host to
    // place dialogs and keeps the default no-op; the standalone host overrides
    // this to pin dialogs to the client-area origin.
    virtual void PositionChildDialog(views::Widget* child_dialog) {}
  };

  explicit PipChildDialogObserverHelper(Delegate* delegate);
  PipChildDialogObserverHelper(const PipChildDialogObserverHelper&) = delete;
  PipChildDialogObserverHelper& operator=(const PipChildDialogObserverHelper&) =
      delete;
  ~PipChildDialogObserverHelper() override;

  // views::WidgetObserver:
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override;
  void OnWidgetDestroying(views::Widget* widget) override;
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override;
  void OnWidgetChildAdded(views::Widget* widget,
                          views::Widget* child_dialog) override;
  void OnWidgetChildRemoved(views::Widget* widget,
                            views::Widget* child_dialog) override;

  // Force any pending child resize to run, rather than waiting for enough
  // wall-clock time to elapse.
  void RunPendingChildResizeForTesting() { RunPendingChildResize(); }
  bool IsChildResizePendingForTesting() const {
    return resize_timer_.IsRunning();
  }

 private:
  enum class ResizingState {
    // We are not currently resized for a child dialog.  For example, if the
    // user manually resizes the pip window, then this is the right state.
    kNotSizedToChildren,

    // A resize due to a child widget is pending.  These are not run
    // immediately because they can happen too quickly.  So, we batch them.
    // This indicates that there's a delayed task pending.
    kPendingResizeForChild,

    // We are in the process of resizing to match a child widget.  While in
    // this state, we will ignore any child resizes to prevent a loop.
    kResizeForChildInProgress,

    // We have finished transitioning to a new size to fit a child dialog and
    // we have not yet returned to the original size (because the child dialog
    // is still open).
    kSizedToChildren,
  };

  // Runs a pending resize immediately if the batch timer is still armed.
  void RunPendingChildResize() {
    if (resize_timer_.IsRunning()) {
      resize_timer_.FireNow();
    }
  }

  // Drops all observation and cached state for `child_dialog`.
  void CleanUpChildDialog(views::Widget* child_dialog);

  // Drops state for a removed child and restores the PiP size. Restoration is
  // posted for bubble dialogs because removing one can happen during its native
  // window's destruction; resizing synchronously would relayout its anchor and
  // attempt to set bounds on the already-destroying window.
  void CleanUpChildDialogAndMaybeRevert(views::Widget* child_dialog);

  // Animates the picture-in-picture child dialogs that are waiting to be
  // resized.
  //
  // When child dialogs that require a resize of the parent window are opened,
  // we first make them transparent and non-interactive to avoid visual glitches
  // while the parent resizes. This function is called after the parent resize
  // is complete to fade in the child dialogs and make them interactive again.
  void AnimateDialogsWaitingForResize();

  void PostResizeForChild(const gfx::Rect& new_bounds);
  void FinishPendingResizeForChild();

  // Resizes the PiP widget so `child_dialog` is not clipped. When
  // `resize_immediately` is true, any resulting pending resize is run
  // synchronously instead of waiting for the batch timer (used when a dialog
  // first appears so it isn't briefly clipped).
  void MaybeResizeForChildDialog(views::Widget* child_dialog,
                                 bool resize_immediately = false);
  void MaybeRevertSizeAfterChildDialogCloses();

  const raw_ptr<Delegate> delegate_;
  const raw_ptr<views::Widget> pip_widget_;

  ResizingState resizing_state_ = ResizingState::kNotSizedToChildren;

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      pip_widget_observation_{this};

  base::ScopedMultiSourceObservation<views::Widget, views::WidgetObserver>
      child_dialog_observations_{this};

  // Tracks child dialogs that have not yet been shown.
  base::flat_set<raw_ptr<views::Widget, CtnExperimental>>
      invisible_child_dialogs_;

  // The bounds that we forced the window to be in response to a child dialog
  // opening.
  gfx::Rect latest_child_dialog_forced_bounds_;

  // The child dialogs that are waiting for the picture-in-picture window to
  // resize.
  //
  // Used to disable visibility changed animations while child dialogs are
  // waiting for the picture-in-picture window to resize. After the
  // picture-in-picture window resizes, or if the user resizes the
  // picture-in-picture window before the resize takes place, this set is used
  // to enable visibility changed animations.
  base::flat_set<raw_ptr<views::Widget>> child_dialogs_waiting_for_resize_;

  // The sizes of the child dialogs. Used to avoid unnecessary resizes.
  base::flat_map<raw_ptr<views::Widget>, gfx::Size> child_dialog_sizes_;

  // The bounds that the window would ideally be if we did not have to enlarge
  // to fit a child dialog.
  gfx::Rect latest_user_desired_bounds_;

  base::OneShotTimer resize_timer_;
  gfx::Rect pending_bounds_;

  base::WeakPtrFactory<PipChildDialogObserverHelper> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PICTURE_IN_PICTURE_PIP_CHILD_DIALOG_OBSERVER_HELPER_H_
