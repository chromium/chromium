// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_ORGANIZER_ORGANIZER_PANEL_CONTROLLER_H_
#define CHROME_BROWSER_UI_TABS_ORGANIZER_ORGANIZER_PANEL_CONTROLLER_H_

#include <optional>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/extension_id.h"
#endif

class BrowserWindowInterface;

namespace actions {
class ActionItem;
}  // namespace actions

class OrganizerPanelController {
 public:
  DECLARE_USER_DATA(OrganizerPanelController);

  explicit OrganizerPanelController(BrowserWindowInterface& browser_window,
                                    actions::ActionItem* root_action_item);
  OrganizerPanelController(const OrganizerPanelController&) = delete;
  OrganizerPanelController& operator=(const OrganizerPanelController&) = delete;
  virtual ~OrganizerPanelController();

  static OrganizerPanelController* From(BrowserWindowInterface* browser_window);

  bool IsOrganizerPanelVisible() const;

  void SetOrganizerVisible(bool visible);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  void OpenForExtension(const extensions::ExtensionId& extension_id);
  void ToggleForExtension(const extensions::ExtensionId& extension_id);
  void CloseForExtension(const extensions::ExtensionId& extension_id);

  const std::optional<extensions::ExtensionId>& active_extension_id() const {
    return active_extension_id_;
  }
#endif

  using StateChangedCallback =
      base::RepeatingCallback<void(OrganizerPanelController*)>;
  base::CallbackListSubscription RegisterOnStateChanged(
      StateChangedCallback callback);

 private:
  // Notifies subscribers when the is_visible_ state of the Organizer Panel
  // changes.
  void NotifyStateChanged();

  // Update the Organizer Button's Action Item (kActionToggleOrganizerPanel)
  // based on the Organizer Panel's is_visible_ state.
  void UpdateOrganizerActionItem();

  // Controls whether the Organizer Panel is visible.
  bool is_visible_ = false;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  std::optional<extensions::ExtensionId> active_extension_id_;
#endif

  const raw_ref<BrowserWindowInterface> browser_window_;
  const raw_ptr<actions::ActionItem> root_action_item_;

  // Records the last time the panel was opened. Used for recording how long the
  // panel was open.
  base::TimeTicks last_opened_time_;

  // Callback list for state changes to the visibility.
  base::RepeatingCallbackList<void(OrganizerPanelController*)>
      on_state_changed_callback_list_;
  ui::ScopedUnownedUserData<OrganizerPanelController> scoped_unowned_user_data_;

  base::WeakPtrFactory<OrganizerPanelController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_TABS_ORGANIZER_ORGANIZER_PANEL_CONTROLLER_H_
