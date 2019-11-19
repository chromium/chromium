// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_MULTI_USER_MULTI_PROFILE_SUPPORT_H_
#define CHROME_BROWSER_UI_ASH_MULTI_USER_MULTI_PROFILE_SUPPORT_H_

#include <map>
#include <memory>

#include "ash/public/cpp/multi_user_window_manager_delegate.h"
#include "base/macros.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_helper.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "components/account_id/account_id.h"

class AppObserver;
class Browser;

namespace ash {
class MultiUserWindowManager;
class MultiProfileSupportTest;
}  // namespace ash

namespace content {
class BrowserContext;
}

namespace aura {
class Window;
}  // namespace aura

// This class acts as a helper to keep ash's MultiUserWindowManager in sync with
// windows created in the browser. For example, this adds all browser windows
// to MultiUserWindowManager as well as all app windows. This class is only
// created if SessionControllerClient::IsMultiProfileAvailable() returns true.
class MultiProfileSupport : public ash::MultiUserWindowManagerDelegate,
                            public BrowserListObserver {
 public:
  // Create the manager and use |active_account_id| as the active user.
  explicit MultiProfileSupport(const AccountId& active_account_id);
  ~MultiProfileSupport() override;

  static MultiProfileSupport* GetInstanceForTest() { return instance_; }

  // Initializes the manager after its creation. Should only be called once.
  void Init();

  void AddUser(content::BrowserContext* context);
  ash::MultiUserWindowManager* multi_user_window_manager() {
    return multi_user_window_manager_.get();
  }
  const ash::MultiUserWindowManager* multi_user_window_manager() const {
    return multi_user_window_manager_.get();
  }

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override;

 private:
  friend class ash::MultiProfileSupportTest;

  // ash::MultiUserWindowManagerDelegate:
  void OnWindowOwnerEntryChanged(aura::Window* window,
                                 const AccountId& account_id,
                                 bool was_minimized,
                                 bool teleported) override;
  void OnTransitionUserShelfToNewAccount() override;

  // The single instance of MultiProfileSupport, tracked solely for
  // tests.
  static MultiProfileSupport* instance_;

  using AccountIdToAppWindowObserver = std::map<AccountId, AppObserver*>;

  // A list of all known users and their app window observers.
  AccountIdToAppWindowObserver account_id_to_app_observer_;

  std::unique_ptr<ash::MultiUserWindowManager> multi_user_window_manager_;

  DISALLOW_COPY_AND_ASSIGN(MultiProfileSupport);
};

#endif  // CHROME_BROWSER_UI_ASH_MULTI_USER_MULTI_PROFILE_SUPPORT_H_
