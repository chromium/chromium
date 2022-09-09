// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_MULTI_USER_MULTI_USER_WINDOW_MANAGER_STUB_H_
#define CHROME_BROWSER_UI_ASH_MULTI_USER_MULTI_USER_WINDOW_MANAGER_STUB_H_

#include "ash/public/cpp/multi_user_window_manager.h"

// Stub implementation of ash::MultiUserWindowManager. This is used for single
// user mode.
class MultiUserWindowManagerStub : public ash::MultiUserWindowManager {
 public:
  MultiUserWindowManagerStub();

  MultiUserWindowManagerStub(const MultiUserWindowManagerStub&) = delete;
  MultiUserWindowManagerStub& operator=(const MultiUserWindowManagerStub&) =
      delete;

  ~MultiUserWindowManagerStub() override;

  // MultiUserWindowManager overrides:
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
};

#endif  // CHROME_BROWSER_UI_ASH_MULTI_USER_MULTI_USER_WINDOW_MANAGER_STUB_H_
