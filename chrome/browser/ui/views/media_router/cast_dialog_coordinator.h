// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_CAST_DIALOG_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_CAST_DIALOG_COORDINATOR_H_

#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions.h"
#include "components/media_router/browser/media_router_metrics.h"
#include "ui/views/bubble/bubble_anchor.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/view_tracker.h"

class BrowserWindowInterface;
class Profile;

namespace actions {
class ActionItem;
}  // namespace actions

namespace gfx {
class Rect;
}  // namespace gfx

namespace views {
class Widget;
}  // namespace views

namespace media_router {

class CastDialogController;
class CastDialogView;

// Result of an attempt to show a cast dialog.
enum class ShowCastDialogStatus {
  // Dialog successfully shown.
  kSuccess,
  // Dialog failed to display because CastDialogController was freed before
  // dialog could be shown.
  kCastCanceled,
  // Dialog failed to display because BrowserWindowInterface was freed before
  // dialog could be shown.
  kWindowClosed,
};

class CastDialogCoordinator {
 public:
  // Callback type called after a dialog is attempted to be shown.
  using AfterShownCallback = base::OnceCallback<void(ShowCastDialogStatus)>;

  CastDialogCoordinator();
  CastDialogCoordinator(const CastDialogCoordinator&) = delete;
  CastDialogCoordinator& operator=(const CastDialogCoordinator&) = delete;
  ~CastDialogCoordinator();

  // Shows the singleton dialog anchored to the Cast toolbar icon. Requires that
  // BrowserActionsContainer exists for |browser|. `after_shown_callback` is
  // called once dialog is shown.
  void ShowDialogWithToolbarAction(
      base::WeakPtr<CastDialogController> controller,
      BrowserWindowInterface* browser,
      const base::Time& start_time,
      MediaRouterDialogActivationLocation activation_location,
      AfterShownCallback after_shown_callback);

  // Shows the singleton dialog anchored to the top-center of the browser
  // window. `after_shown_callback` is called once dialog is shown.
  void ShowDialogCenteredForBrowserWindow(
      CastDialogController* controller,
      BrowserWindowInterface* browser,
      const base::Time& start_time,
      MediaRouterDialogActivationLocation activation_location,
      AfterShownCallback after_shown_callback);

  // Shows the singleton dialog anchored to the bottom of |bounds|, horizontally
  // centered. `after_shown_callback` is called once dialog is shown.
  // TODO(crbug.com/1345683): This should be removed if there are no call paths
  // in production that result in this method being called. Creating a bubble
  // would DCHECK in the current code with a nullptr anchor view and no
  // additional handling.
  void ShowDialogCentered(
      const gfx::Rect& bounds,
      CastDialogController* controller,
      Profile* profile,
      const base::Time& start_time,
      MediaRouterDialogActivationLocation activation_location,
      AfterShownCallback after_shown_callback);

  // No-op if the dialog is currently not shown.
  void Hide();

  bool IsShowing() const;

  CastDialogView* GetCastDialogView();

  views::Widget* GetCastDialogWidget();

 private:
  // TODO(crbug.com/1346127): Remove friend class.
  friend class CastDialogViewTest;

  // Instantiates and shows the singleton dialog. The dialog must not be
  // currently shown.
  void Show(views::BubbleAnchor anchor,
            views::BubbleBorder::Arrow anchor_position,
            CastDialogController* controller,
            Profile* profile,
            const base::Time& start_time,
            MediaRouterDialogActivationLocation activation_location,
            actions::ActionItem* action_item,
            AfterShownCallback after_shown_callback);

  // Once the anchor for ShowDialogWithToolbarAction() becomes visible, this
  // method is called to actually show the dialog anchored to `anchor`.
  // `activation_location` is where the user click to open the dialog.
  // `start_time` is the time when the dialog creation process started and is
  // used to calculate metrics about follow-up events.
  // `after_shown_callback` is called once dialog is shown.
  void OnBubbleAnchorVisible(
      base::WeakPtr<CastDialogController> controller,
      base::WeakPtr<BrowserWindowInterface> browser,
      const base::Time& start_time,
      MediaRouterDialogActivationLocation activation_location,
      AfterShownCallback after_shown_callback,
      base::expected<views::BubbleAnchor, GetAnchorFailureReason> anchor);

  views::ViewTracker cast_dialog_view_tracker_;

  base::WeakPtrFactory<CastDialogCoordinator> weak_ptr_factory_{this};
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_CAST_DIALOG_COORDINATOR_H_
