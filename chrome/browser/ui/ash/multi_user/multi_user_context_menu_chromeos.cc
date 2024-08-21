// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/multi_user/multi_user_context_menu.h"

#include "ash/public/cpp/multi_user_window_manager.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_helper.h"
#include "chrome/browser/ui/ash/session/session_controller_client_impl.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_manager_pref_names.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"

namespace {

class MultiUserContextMenuChromeos : public ui::SimpleMenuModel,
                                     public ui::SimpleMenuModel::Delegate {
 public:
  explicit MultiUserContextMenuChromeos(aura::Window* window);

  MultiUserContextMenuChromeos(const MultiUserContextMenuChromeos&) = delete;
  MultiUserContextMenuChromeos& operator=(const MultiUserContextMenuChromeos&) =
      delete;

  ~MultiUserContextMenuChromeos() override {}

  // SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int command_id) const override { return false; }
  bool IsCommandIdEnabled(int command_id) const override { return true; }
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  // The window for which this menu is.
  raw_ptr<aura::Window> window_;
};

MultiUserContextMenuChromeos::MultiUserContextMenuChromeos(aura::Window* window)
    : ui::SimpleMenuModel(this), window_(window) {}

void MultiUserContextMenuChromeos::ExecuteCommand(int command_id,
                                                  int event_flags) {
  ExecuteVisitDesktopCommand(command_id, window_);
}

void OnAcceptTeleportWarning(const AccountId& account_id,
                             aura::Window* window_,
                             bool accepted,
                             bool no_show_again) {
  if (!accepted)
    return;

  PrefService* pref = ProfileManager::GetActiveUserProfile()->GetPrefs();
  pref->SetBoolean(user_manager::prefs::kMultiProfileWarningShowDismissed,
                   no_show_again);

  MultiUserWindowManagerHelper::GetWindowManager()->ShowWindowForUser(
      window_, account_id);
}

}  // namespace

std::unique_ptr<ui::MenuModel> CreateMultiUserContextMenu(
    aura::Window* window) {
  std::unique_ptr<ui::MenuModel> model;
  const user_manager::UserList logged_in_users =
      user_manager::UserManager::Get()->GetLRULoggedInUsers();

  if (logged_in_users.size() > 1u) {
    // If this window is not owned, we don't show the menu addition.
    auto* window_manager = MultiUserWindowManagerHelper::GetWindowManager();
    const AccountId& account_id = window_manager->GetWindowOwner(window);
    if (!account_id.is_valid() || !window)
      return model;
    auto* menu = new MultiUserContextMenuChromeos(window);
    model.reset(menu);
    int command_id = IDC_VISIT_DESKTOP_OF_LRU_USER_NEXT;
    for (size_t user_index = 1; user_index < logged_in_users.size();
         ++user_index) {
      if (command_id > IDC_VISIT_DESKTOP_OF_LRU_USER_LAST) {
        break;
      }
      const user_manager::User* user_info = logged_in_users[user_index];
      menu->AddItem(
          command_id,
          l10n_util::GetStringFUTF16(
              IDS_VISIT_DESKTOP_OF_LRU_USER, user_info->GetDisplayName(),
              base::ASCIIToUTF16(user_info->GetDisplayEmail())));
      ++command_id;
    }
  }
  return model;
}

void ExecuteVisitDesktopCommand(int command_id, aura::Window* window) {
  switch (command_id) {
    case IDC_VISIT_DESKTOP_OF_LRU_USER_2:
    case IDC_VISIT_DESKTOP_OF_LRU_USER_3:
    case IDC_VISIT_DESKTOP_OF_LRU_USER_4:
    case IDC_VISIT_DESKTOP_OF_LRU_USER_5: {
      const user_manager::UserList logged_in_users =
          user_manager::UserManager::Get()->GetLRULoggedInUsers();
      // When running the multi user mode on Chrome OS, windows can "visit"
      // another user's desktop.
      const AccountId account_id =
          logged_in_users[command_id - IDC_VISIT_DESKTOP_OF_LRU_USER_NEXT + 1]
              ->GetAccountId();
      base::OnceCallback<void(bool, bool)> on_accept =
          base::BindOnce(&OnAcceptTeleportWarning, account_id, window);

      // Don't show warning dialog if any logged in user in multi-profiles
      // session dismissed it.
      for (user_manager::UserList::const_iterator it = logged_in_users.begin();
           it != logged_in_users.end(); ++it) {
        if (multi_user_util::GetProfileFromAccountId((*it)->GetAccountId())
                ->GetPrefs()
                ->GetBoolean(
                    user_manager::prefs::kMultiProfileWarningShowDismissed)) {
          bool active_user_show_option =
              ProfileManager::GetActiveUserProfile()->GetPrefs()->GetBoolean(
                  user_manager::prefs::kMultiProfileWarningShowDismissed);
          std::move(on_accept).Run(true, active_user_show_option);
          return;
        }
      }

      SessionControllerClientImpl::Get()->ShowTeleportWarningDialog(
          std::move(on_accept));
      return;
    }
    default:
      NOTREACHED_IN_MIGRATION();
  }
}
