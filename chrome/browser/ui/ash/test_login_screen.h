// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_TEST_LOGIN_SCREEN_H_
#define CHROME_BROWSER_UI_ASH_TEST_LOGIN_SCREEN_H_

#include <string>
#include <vector>

#include "ash/public/cpp/login_screen.h"
#include "base/macros.h"
#include "chrome/browser/ui/ash/test_login_screen_model.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace ash {
class ScopedGuestButtonBlocker;
}

// Test implementation of ash's mojo LoginScreen interface.
//
// Registers itself to ServiceManager on construction and deregisters
// on destruction.
//
// Note: A ServiceManagerConnection must be initialized before constructing this
// object. Consider using content::TestServiceManagerContext on your tests.
class TestLoginScreen : public ash::LoginScreen {
 public:
  TestLoginScreen();
  ~TestLoginScreen() override;

  // ash::LoginScreen:
  void SetClient(ash::LoginScreenClient* client) override;
  ash::LoginScreenModel* GetModel() override;
  void ShowLockScreen() override;
  void ShowLoginScreen() override;
  void ShowKioskAppError(const std::string& message) override;
  void FocusLoginShelf(bool reverse) override;
  bool IsReadyForPassword() override;
  void EnableAddUserButton(bool enable) override;
  void EnableShutdownButton(bool enable) override;
  void ShowGuestButtonInOobe(bool show) override;
  void ShowParentAccessButton(bool show) override;
  void ShowParentAccessWidget(
      const AccountId& child_account_id,
      base::RepeatingCallback<void(bool success)> callback,
      ash::ParentAccessRequestReason reason,
      bool extra_dimmer,
      base::Time validation_time) override;
  void SetAllowLoginAsGuest(bool allow_guest) override;
  std::unique_ptr<ash::ScopedGuestButtonBlocker> GetScopedGuestButtonBlocker()
      override;
  void RequestSecurityTokenPin(ash::SecurityTokenPinRequest request) override;
  void ClearSecurityTokenPinRequest() override;

 private:
  TestLoginScreenModel test_screen_model_;

  DISALLOW_COPY_AND_ASSIGN(TestLoginScreen);
};

#endif  // CHROME_BROWSER_UI_ASH_TEST_LOGIN_SCREEN_H_
