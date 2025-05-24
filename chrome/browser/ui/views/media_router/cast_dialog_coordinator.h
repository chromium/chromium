// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_CAST_DIALOG_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_CAST_DIALOG_COORDINATOR_H_

#include "base/time/time.h"
#include "components/media_router/browser/media_router_metrics.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/view_tracker.h"

class Browser;
class Profile;

namespace actions {
class ActionItem;
}  // namespace actions

namespace gfx {
class Rect;
}  // namespace gfx

namespace views {
class View;
class Widget;
}  // namespace views

namespace media_router {

class CastDialogController;
class CastDialogView;

class CastDialogCoordinator {
 public:
  CastDialogCoordinator() = default;
  CastDialogCoordinator(const CastDialogCoordinator&) = delete;
  CastDialogCoordinator& operator=(const CastDialogCoordinator&) = delete;
  ~CastDialogCoordinator() = default;

  // Shows the singleton dialog anchored to the Cast toolbar icon. Requires that
  // BrowserActionsContainer exists for |browser|.
  void ShowDialogWithToolbarAction(
      CastDialogController* controller,
      Browser* browser,
      const base::Time& start_time,
      MediaRouterDialogActivationLocation activation_location);

  // Shows the singleton dialog anchored to the top-center of the browser
  // window.
  void ShowDialogCenteredForBrowserWindow(
      CastDialogController* controller,
      Browser* browser,
      const base::Time& start_time,
      MediaRouterDialogActivationLocation activation_location);

  // Shows the singleton dialog anchored to the bottom of |bounds|, horizontally
  // centered.
  // TODO(crbug.com/1345683): This should be removed if there are no call paths
  // in production that result in this method being called. Creating a bubble
  // would DCHECK in the current code with a nullptr anchor view and no
  // additional handling.
  void ShowDialogCentered(
      const gfx::Rect& bounds,
      CastDialogController* controller,
      Profile* profile,
      const base::Time& start_time,
      MediaRouterDialogActivationLocation activation_location);

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
  void Show(views::View* anchor_view,
            views::BubbleBorder::Arrow anchor_position,
            CastDialogController* controller,
            Profile* profile,
            const base::Time& start_time,
            MediaRouterDialogActivationLocation activation_location,
            actions::ActionItem* action_item = nullptr);

  views::ViewTracker cast_dialog_view_tracker_;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_CAST_DIALOG_COORDINATOR_H_
