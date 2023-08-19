// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_MULTI_USER_TEST_MULTI_USER_WINDOW_MANAGER_H_
#define CHROME_BROWSER_UI_ASH_MULTI_USER_TEST_MULTI_USER_WINDOW_MANAGER_H_

#include "ash/public/cpp/multi_user_window_manager.h"
#include "base/memory/raw_ptr.h"
#include "components/account_id/account_id.h"

class Browser;

// This is a test implementation of a MultiUserWindowManager which allows to
// test a visiting window on another desktop. It will install and remove itself
// from the system upon creation / destruction. The creation function gets a
// |browser| which is shown on |desktop_owner|'s desktop.
class TestMultiUserWindowManager : public ash::MultiUserWindowManager {
 public:
  TestMultiUserWindowManager(const TestMultiUserWindowManager&) = delete;
  TestMultiUserWindowManager& operator=(const TestMultiUserWindowManager&) =
      delete;

  ~TestMultiUserWindowManager() override;

  // Creates an installs TestMultiUserWindowManager as the
  // MultiUserWindowManager used by MultiUserWindowManagerHelper. The returned
  // value is owned by MultiUserWindowManagerHelper.
  static TestMultiUserWindowManager* Create(Browser* visiting_browser,
                                            const AccountId& desktop_owner);

  aura::Window* created_window() { return created_window_; }

  // ash::MultiUserWindowManager overrides:
  void SetWindowOwner(aura::Window* window,
                      const AccountId& account_id) override;
  const AccountId& GetWindowOwner(const aura::Window* window) const override;
  void ShowWindowForUser(aura::Window* window,
                         const AccountId& account_id) override;
  bool AreWindowsSharedAmongUsers() const override;
  std::set<AccountId> GetOwnersOfVisibleWindows() const override;
  const AccountId& GetUserPresentingWindow(
      const aura::Window* window) const override;
  const AccountId& CurrentAccountId() const override;

 private:
  TestMultiUserWindowManager(Browser* visiting_browser,
                             const AccountId& desktop_owner);

  // The window of the visiting browser.
  raw_ptr<aura::Window, AcrossTasksDanglingUntriaged> browser_window_;
  // The owner of the visiting browser.
  AccountId browser_owner_;
  // The owner of the currently shown desktop.
  AccountId desktop_owner_;
  // The created window.
  raw_ptr<aura::Window, AcrossTasksDanglingUntriaged> created_window_ = nullptr;
  // The location of the window.
  AccountId created_window_shown_for_;
  // The current selected active user.
  AccountId current_account_id_;
};

#endif  // CHROME_BROWSER_UI_ASH_MULTI_USER_TEST_MULTI_USER_WINDOW_MANAGER_H_
