// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORDS_PAGE_ACTION_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORDS_PAGE_ACTION_CONTROLLER_H_

#include <string>

#include "base/memory/raw_ref.h"
#include "components/password_manager/core/common/password_manager_ui.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace page_actions {
class PageActionController;
}  // namespace page_actions

namespace tabs {
class TabInterface;
}  // namespace tabs

class ManagePasswordsUIController;

namespace actions {
class ActionItem;
}  // namespace actions

// Controller for the manage passwords page action icon. This class observes
// the state of ManagePasswordsUIController and updates the page action icon
// visibility, icon, and tooltip accordingly.
class ManagePasswordsPageActionController {
 public:
  DECLARE_USER_DATA(ManagePasswordsPageActionController);

  ManagePasswordsPageActionController(
      tabs::TabInterface& tab_interface,
      page_actions::PageActionController& page_action_controller);
  ~ManagePasswordsPageActionController();

  ManagePasswordsPageActionController(
      const ManagePasswordsPageActionController&) = delete;
  ManagePasswordsPageActionController& operator=(
      const ManagePasswordsPageActionController&) = delete;

  // Returns the controller associated with `tab`, or nullptr if `tab` is null
  // or no controller has been attached to it.
  static ManagePasswordsPageActionController* From(tabs::TabInterface* tab);

  // Fetches the current password state and updates the page action icon.
  // `state`: The current password_manager::ui::State.
  void UpdateVisibility(password_manager::ui::State state,
                        ManagePasswordsUIController& passwords_ui_controller,
                        actions::ActionItem& passwords_action_item);

  // Returns the appropriate tooltip text for the manage passwords icon based
  // on the current state.
  static std::u16string GetManagePasswordsTooltipText(
      password_manager::ui::State state);

 private:
  const raw_ref<page_actions::PageActionController> page_action_controller_;
  ui::ScopedUnownedUserData<ManagePasswordsPageActionController>
      scoped_unowned_user_data_;
};
#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_MANAGE_PASSWORDS_PAGE_ACTION_CONTROLLER_H_
