// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_MULTI_USER_MULTI_USER_WINDOW_MANAGER_HELPER_H_
#define CHROME_BROWSER_UI_ASH_MULTI_USER_MULTI_USER_WINDOW_MANAGER_HELPER_H_

#include <map>
#include <memory>
#include <set>

#include "base/auto_reset.h"

class AccountId;

namespace ash {
class MultiUserWindowManager;
class MultiUserWindowManagerBrowserAdaptor;
}  // namespace ash

// MultiUserWindowManagerHelper is responsible for creating and owning the
// right ash::MultiUserWindowManager implementation. If multi-profile is not
// enabled it creates a stub implementation, otherwise
// MultiUserWindowManagerBrowserAdaptor, which internally owns the real
// ash::MultiUserWindowManager implementation.
class MultiUserWindowManagerHelper {
 public:
  MultiUserWindowManagerHelper(const MultiUserWindowManagerHelper&) = delete;
  MultiUserWindowManagerHelper& operator=(const MultiUserWindowManagerHelper&) =
      delete;

  // Creates an instance of the MultiUserWindowManagerHelper.
  static MultiUserWindowManagerHelper* CreateInstance();

  // Gets the instance of the object.
  static MultiUserWindowManagerHelper* GetInstance();

  // Removes the instance.
  static void DeleteInstance();

  // Used in tests to create an instance with
  // MultiUserWindowManagerBrowserAdaptor configured for |account_id|.
  static void CreateInstanceForTest();

  // Returns true if MultiUserSignIn is enabled. Always true on production.
  static bool IsEnabled();

  // Temporarily disables MultiUserSignIn for testing purpose.
  // On destruction of the returned AutoReset instance, disabling is reset
  // (so the following tests will run with MultiUserSignIn).
  [[nodiscard]] static base::AutoReset<bool> DisableForTesting();

  // Adds user to monitor starting and running V1/V2 application windows.
  // Returns immediately if the user (identified by a |profile|) is already
  // known to the manager. Note: This function is not implemented as a
  // SessionStateObserver to coordinate the timing of the addition with other
  // modules.
  // This must be called after User's profile gets ready.
  void AddUser(const AccountId& account_id);

 private:
  MultiUserWindowManagerHelper();
  ~MultiUserWindowManagerHelper();

  // Used in multi-profile support.
  std::unique_ptr<ash::MultiUserWindowManagerBrowserAdaptor>
      multi_user_window_manager_browser_adaptor_;
};

#endif  // CHROME_BROWSER_UI_ASH_MULTI_USER_MULTI_USER_WINDOW_MANAGER_HELPER_H_
