// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/login_ui_service.h"

#include "testing/gtest/include/gtest/gtest.h"

class TestLoginUI : public LoginUIService::LoginUI {
 public:
  TestLoginUI() = default;

  TestLoginUI(const TestLoginUI&) = delete;
  TestLoginUI& operator=(const TestLoginUI&) = delete;

  ~TestLoginUI() override = default;
  void FocusUI() override {}
};

TEST(LoginUIServiceTest, CanSetMultipleLoginUIs) {
  LoginUIService service;

  EXPECT_EQ(nullptr, service.current_login_ui());

  TestLoginUI ui;
  service.SetLoginUI(&ui);
  EXPECT_EQ(&ui, service.current_login_ui());

  // Test that we can replace the active login UI.
  TestLoginUI other_ui;
  service.SetLoginUI(&other_ui);
  EXPECT_EQ(&other_ui, service.current_login_ui());

  // Test that closing the non-active login UI has no effect.
  service.LoginUIClosed(&ui);
  EXPECT_EQ(&other_ui, service.current_login_ui());

  // Test that closing the foreground UI yields the background UI.
  service.SetLoginUI(&ui);
  EXPECT_EQ(&ui, service.current_login_ui());
  service.LoginUIClosed(&ui);
  EXPECT_EQ(&other_ui, service.current_login_ui());

  // Test that closing the last login UI makes the current login UI nullptr.
  service.LoginUIClosed(&other_ui);
  EXPECT_EQ(nullptr, service.current_login_ui());
}
