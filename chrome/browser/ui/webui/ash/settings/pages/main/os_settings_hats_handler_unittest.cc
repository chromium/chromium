// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/main/os_settings_hats_handler.h"
#include "chrome/browser/ui/webui/ash/settings/services/hats/os_settings_hats_manager.h"
#include "chrome/browser/ui/webui/ash/settings/services/hats/os_settings_hats_manager_factory.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::settings {

class TestSettingsHatsHandler : public OsSettingsHatsHandler {
 public:
  explicit TestSettingsHatsHandler(TestingProfile* testing_profile)
      : OsSettingsHatsHandler(testing_profile) {}

  // Make public for testing.
  using OsSettingsHatsHandler::set_web_ui;
};

class MockOsSettingsHatsManager : public OsSettingsHatsManager {
 public:
  explicit MockOsSettingsHatsManager(content::BrowserContext* browser_context)
      : OsSettingsHatsManager(browser_context) {}

  MOCK_METHOD(void, SetSettingsUsedSearch, (bool has_user_used_search));
  MOCK_METHOD(void, MaybeSendSettingsHats, ());
};

static std::unique_ptr<KeyedService> MockManagerFactory(
    content::BrowserContext* context) {
  return std::make_unique<MockOsSettingsHatsManager>(context);
}

class SettingsHatsHandlerTest : public testing::Test {
 public:
  SettingsHatsHandlerTest() = default;
  ~SettingsHatsHandlerTest() override = default;

  void SetUp() override {
    manager_handler_ = static_cast<MockOsSettingsHatsManager*>(
        OsSettingsHatsManagerFactory::GetInstance()->SetTestingFactoryAndUse(
            &profile_,
            base::BindRepeating([](content::BrowserContext* context) {
              return MockManagerFactory(context);
            })));

    handler_ = std::make_unique<TestSettingsHatsHandler>(&profile_);
    handler_->set_web_ui(&web_ui_);
    handler_->RegisterMessages();
    handler_->AllowJavascriptForTesting();
  }

  void TearDown() override { handler_.reset(); }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      content::BrowserTaskEnvironment::REAL_IO_THREAD};
  TestingProfile profile_;
  content::TestWebUI web_ui_;
  std::unique_ptr<TestSettingsHatsHandler> handler_;
  raw_ptr<MockOsSettingsHatsManager> manager_handler_;
};

TEST_F(SettingsHatsHandlerTest, TestHandleSettingsUsedSearch) {
  base::Value::List args;
  EXPECT_CALL(*manager_handler_, SetSettingsUsedSearch(true)).Times(1);
  web_ui_.HandleReceivedMessage("settingsUsedSearch", args);
}

TEST_F(SettingsHatsHandlerTest, TestHandleSendSettingsHats) {
  base::Value::List args;
  EXPECT_CALL(*manager_handler_, MaybeSendSettingsHats()).Times(1);
  web_ui_.HandleReceivedMessage("sendSettingsHats", args);
}
}  // namespace ash::settings
