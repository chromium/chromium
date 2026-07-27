// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_ORGANIZER_ORGANIZER_PANEL_STATE_CONTROLLER_H_
#define CHROME_BROWSER_UI_TABS_ORGANIZER_ORGANIZER_PANEL_STATE_CONTROLLER_H_

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;

namespace actions {
class ActionItem;
}  // namespace actions

class OrganizerPanelStateController {
 public:
  DECLARE_USER_DATA(OrganizerPanelStateController);

  explicit OrganizerPanelStateController(BrowserWindowInterface* browser_window,
                                         actions::ActionItem* root_action_item);
  OrganizerPanelStateController(const OrganizerPanelStateController&) = delete;
  OrganizerPanelStateController& operator=(
      const OrganizerPanelStateController&) = delete;
  virtual ~OrganizerPanelStateController();

  static OrganizerPanelStateController* From(
      BrowserWindowInterface* browser_window);

  bool IsOrganizerPanelVisible() const;

  void SetOrganizerVisible(bool visible);

  using StateChangedCallback =
      base::RepeatingCallback<void(OrganizerPanelStateController*)>;
  base::CallbackListSubscription RegisterOnStateChanged(
      StateChangedCallback callback);

 private:
  // Notifies subscribers when the is_visible_ state of the Organizer Panel
  // changes.
  void NotifyStateChanged();

  // Update the Organizer Button's Action Item (kActionToggleOrganizerPanel)
  // based on the Organizer Panel's is_visible_ state.
  void UpdateOrganizerActionItem();

  // Controls whether the OrganizerPanelView is visible.
  bool is_visible_ = false;

  const raw_ptr<actions::ActionItem> root_action_item_;

  // Callback list for state changes to the visibility.
  base::RepeatingCallbackList<void(OrganizerPanelStateController*)>
      on_state_changed_callback_list_;
  ui::ScopedUnownedUserData<OrganizerPanelStateController>
      scoped_unowned_user_data_;

  base::WeakPtrFactory<OrganizerPanelStateController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_TABS_ORGANIZER_ORGANIZER_PANEL_STATE_CONTROLLER_H_
