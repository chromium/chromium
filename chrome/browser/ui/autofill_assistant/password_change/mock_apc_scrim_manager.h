// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_MOCK_APC_SCRIM_MANAGER_H_
#define CHROME_BROWSER_UI_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_MOCK_APC_SCRIM_MANAGER_H_

#include "chrome/browser/ui/autofill_assistant/password_change/apc_scrim_manager.h"

#include "testing/gmock/include/gmock/gmock.h"

// Mocked ApcScrimManager used in unit tests.
class MockApcScrimManager : public ApcScrimManager {
 public:
  MockApcScrimManager();
  ~MockApcScrimManager() override;

  MOCK_METHOD(void, Show, (), (override));
  MOCK_METHOD(void, Hide, (), (override));
  MOCK_METHOD(bool, GetVisible, (), (override));
};

#endif  // CHROME_BROWSER_UI_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_MOCK_APC_SCRIM_MANAGER_H_
