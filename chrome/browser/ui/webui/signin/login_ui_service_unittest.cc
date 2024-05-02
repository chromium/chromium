// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/login_ui_service.h"

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/signin/signin_ui_error.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension_builder.h"
#include "google_apis/gaia/gaia_urls.h"
#endif

class LoginUIServiceTest : public testing::Test {
 public:
  LoginUIServiceTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()),
        profile_(nullptr) {}

  LoginUIServiceTest(const LoginUIServiceTest&) = delete;
  LoginUIServiceTest& operator=(const LoginUIServiceTest&) = delete;

  ~LoginUIServiceTest() override {}

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("Person 1");
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;

  TestingProfileManager profile_manager_;
  // Test profile used by all tests - this is owned by profile_manager_.
  raw_ptr<TestingProfile> profile_;
};

class TestLoginUI : public LoginUIService::LoginUI {
 public:
  TestLoginUI() { }

  TestLoginUI(const TestLoginUI&) = delete;
  TestLoginUI& operator=(const TestLoginUI&) = delete;

  ~TestLoginUI() override {}
  void FocusUI() override {}
};

TEST_F(LoginUIServiceTest, CanSetMultipleLoginUIs) {
  LoginUIService service(profile_);

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

#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(LoginUIServiceTest, SetProfileBlockingErrorMessage) {
  LoginUIService service(profile_);

  service.SetProfileBlockingErrorMessage();

  EXPECT_EQ(service.GetLastLoginError(), SigninUIError::ProfileIsBlocked());
}
#endif
