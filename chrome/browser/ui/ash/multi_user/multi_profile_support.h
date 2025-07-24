// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_MULTI_USER_MULTI_PROFILE_SUPPORT_H_
#define CHROME_BROWSER_UI_ASH_MULTI_USER_MULTI_PROFILE_SUPPORT_H_

#include <map>
#include <memory>

#include "ash/public/cpp/multi_user_window_manager_observer.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_helper.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "components/account_id/account_id.h"

class AppObserver;
class Browser;

namespace ash {
class MultiUserWindowManager;
class MultiProfileSupportTest;
}  // namespace ash

namespace aura {
class Window;
}  // namespace aura

// This class acts as a helper to keep ash's MultiUserWindowManager in sync with
// windows created in the browser. For example, this adds all browser windows
// to MultiUserWindowManager as well as all app windows. This class is only
// created if SessionControllerClient::IsMultiProfileAvailable() returns true.
class MultiProfileSupport : public ash::MultiUserWindowManagerObserver,
                            public BrowserListObserver {
 public:
  // Create the manager and use |active_account_id| as the active user.
  explicit MultiProfileSupport(
      ash::MultiUserWindowManager* multi_user_window_manager);

  MultiProfileSupport(const MultiProfileSupport&) = delete;
  MultiProfileSupport& operator=(const MultiProfileSupport&) = delete;

  ~MultiProfileSupport() override;

  void AddUser(const AccountId& account_id);

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override;

 private:
  using AccountIdToAppWindowObserver =
      std::map<AccountId, std::unique_ptr<AppObserver>>;

  friend class ash::MultiProfileSupportTest;

  // ash::MultiUserWindowManagerObserver:
  void OnWindowOwnerEntryChanged(aura::Window* window,
                                 const AccountId& account_id,
                                 bool was_minimized,
                                 bool teleported) override;
  void OnTransitionUserShelfToNewAccount() override;

  const raw_ptr<ash::MultiUserWindowManager> multi_user_window_manager_;
  base::ScopedObservation<ash::MultiUserWindowManager,
                          ash::MultiUserWindowManagerObserver>
      multi_user_window_manager_observation_{this};

  // A list of all known users and their app window observers.
  AccountIdToAppWindowObserver account_id_to_app_observer_;
};

#endif  // CHROME_BROWSER_UI_ASH_MULTI_USER_MULTI_PROFILE_SUPPORT_H_
