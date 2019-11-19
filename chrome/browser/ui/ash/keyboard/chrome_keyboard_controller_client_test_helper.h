// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_KEYBOARD_CHROME_KEYBOARD_CONTROLLER_CLIENT_TEST_HELPER_H_
#define CHROME_BROWSER_UI_ASH_KEYBOARD_CHROME_KEYBOARD_CONTROLLER_CLIENT_TEST_HELPER_H_

#include <memory>

#include "base/callback_forward.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"

class Profile;

// Helper for tests depending on ChromeKeyboardControllerClient.
// Tests must include a content::TestServiceManagerContext instance.
class ChromeKeyboardControllerClientTestHelper {
 public:
  // Use this for tests using ChromeAshTestBase. TODO(stevenjb): Update tests to
  // rely on the fake behavior instead.
  static std::unique_ptr<ChromeKeyboardControllerClientTestHelper>
  InitializeForAsh();

  // Use this for tests that trigger calls to ChromeKeyboardControllerClient.
  // The interface will be connected to a fake implementation.
  static std::unique_ptr<ChromeKeyboardControllerClientTestHelper>
  InitializeWithFake();

  ChromeKeyboardControllerClientTestHelper();
  ~ChromeKeyboardControllerClientTestHelper();

  void SetProfile(Profile* profile);

 private:
  class FakeKeyboardController;

  void Initialize(ash::KeyboardController* keyboard_controller);

  std::unique_ptr<ChromeKeyboardControllerClient>
      chrome_keyboard_controller_client_;

  // Used when InitializeWithFake is called.
  std::unique_ptr<FakeKeyboardController> fake_controller_;

  DISALLOW_COPY_AND_ASSIGN(ChromeKeyboardControllerClientTestHelper);
};

#endif  // CHROME_BROWSER_UI_ASH_KEYBOARD_CHROME_KEYBOARD_CONTROLLER_CLIENT_TEST_HELPER_H_
