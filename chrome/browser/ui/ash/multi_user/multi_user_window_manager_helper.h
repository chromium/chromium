// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_MULTI_USER_MULTI_USER_WINDOW_MANAGER_HELPER_H_
#define CHROME_BROWSER_UI_ASH_MULTI_USER_MULTI_USER_WINDOW_MANAGER_HELPER_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/macros.h"

class AccountId;
class MultiProfileSupport;

namespace ash {
class MultiUserWindowManager;
}

namespace content {
class BrowserContext;
}

namespace aura {
class Window;
}

// MultiUserWindowManagerHelper is responsible for creating and owning the
// right ash::MultiUserWindowManager implementation. If multi-profile is not
// enabled it creates a stub implementation, otherwise MultiProfileSupport,
// which internally owns the real ash::MultiUserWindowManager implementation.
class MultiUserWindowManagerHelper {
 public:
  // Creates an instance of the MultiUserWindowManagerHelper.
  static MultiUserWindowManagerHelper* CreateInstance();

  // Gets the instance of the object.
  static MultiUserWindowManagerHelper* GetInstance();

  static ash::MultiUserWindowManager* GetWindowManager();

  // Whether or not the window's title should show the avatar. On chromeos,
  // this is true when the owner of the window is different from the owner of
  // the desktop.
  static bool ShouldShowAvatar(aura::Window* window);

  // Removes the instance.
  static void DeleteInstance();

  // Used in tests to create an instance with MultiProfileSupport configured for
  // |account_id|.
  static void CreateInstanceForTest(const AccountId& account_id);

  // Used in tests that want to supply a specific ash::MultiUserWindowManager
  // implementation.
  static void CreateInstanceForTest(
      std::unique_ptr<ash::MultiUserWindowManager> window_manager);

  // Adds user to monitor starting and running V1/V2 application windows.
  // Returns immediately if the user (identified by a |profile|) is already
  // known to the manager. Note: This function is not implemented as a
  // SessionStateObserver to coordinate the timing of the addition with other
  // modules.
  void AddUser(content::BrowserContext* profile);

  // A query call for a given window to see if it is on the given user's
  // desktop.
  bool IsWindowOnDesktopOfUser(aura::Window* window,
                               const AccountId& account_id) const;

 private:
  explicit MultiUserWindowManagerHelper(const AccountId& account_id);
  explicit MultiUserWindowManagerHelper(
      std::unique_ptr<ash::MultiUserWindowManager> window_manager);
  ~MultiUserWindowManagerHelper();

  ash::MultiUserWindowManager* GetWindowManagerImpl() {
    return const_cast<ash::MultiUserWindowManager*>(
        const_cast<const MultiUserWindowManagerHelper*>(this)
            ->GetWindowManagerImpl());
  }
  const ash::MultiUserWindowManager* GetWindowManagerImpl() const;

  // Used in multi-profile support.
  std::unique_ptr<MultiProfileSupport> multi_profile_support_;

  // The MultiUserWindowManager implementation to use. If null, the
  // MultiUserWindowManager comes from |multi_profile_support_|.
  std::unique_ptr<ash::MultiUserWindowManager> multi_user_window_manager_;

  DISALLOW_COPY_AND_ASSIGN(MultiUserWindowManagerHelper);
};

#endif  // CHROME_BROWSER_UI_ASH_MULTI_USER_MULTI_USER_WINDOW_MANAGER_HELPER_H_
