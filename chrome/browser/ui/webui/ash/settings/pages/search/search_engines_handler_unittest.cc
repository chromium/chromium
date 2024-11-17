// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/search_engines_handler.h"

#include "ash/public/cpp/test/test_new_window_delegate.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::settings {

namespace {

class MockNewWindowDelegate : public testing::NiceMock<TestNewWindowDelegate> {
 public:
  // TestNewWindowDelegate:
  MOCK_METHOD(void,
              OpenUrl,
              (const GURL& url, OpenUrlFrom from, Disposition disposition),
              (override));
};

class SearchEnginesHandlerAshTest : public testing::Test {
 public:
  SearchEnginesHandlerAshTest() = default;
  SearchEnginesHandlerAshTest(const SearchEnginesHandlerAshTest&) = delete;
  SearchEnginesHandlerAshTest& operator=(const SearchEnginesHandlerAshTest&) =
      delete;
  ~SearchEnginesHandlerAshTest() override = default;

  void SetUp() override {
    // Initialize profile.
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    Profile* profile = profile_manager_->CreateTestingProfile(
        chrome::kInitialProfile,
        {TestingProfile::TestingFactory{
            TemplateURLServiceFactory::GetInstance(),
            base::BindRepeating(
                &TemplateURLServiceFactory::BuildInstanceFor)}});

    // Initialize handler and webui.
    auto handler = std::make_unique<::settings::SearchEnginesHandler>(profile);
    handler_ = handler.get();
    web_ui_ = std::make_unique<content::TestWebUI>();
    web_ui_->AddMessageHandler(std::move(handler));
    handler_->AllowJavascriptForTesting();
  }

  void TearDown() override {
    web_ui_.reset();
  }

 protected:
  MockNewWindowDelegate& new_window_delegate() { return new_window_delegate_; }
  std::unique_ptr<content::TestWebUI> web_ui_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  raw_ptr<::settings::SearchEnginesHandler, DanglingUntriaged> handler_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  MockNewWindowDelegate new_window_delegate_;
};

TEST_F(SearchEnginesHandlerAshTest, OpenBrowserSearchSettings) {
  EXPECT_CALL(
      new_window_delegate(),
      OpenUrl(
          GURL(chrome::kChromeUISettingsURL).Resolve(chrome::kSearchSubPage),
          ash::NewWindowDelegate::OpenUrlFrom::kUserInteraction,
          ash::NewWindowDelegate::Disposition::kSwitchToTab));
  base::Value::List empty_args;
  web_ui_->HandleReceivedMessage("openBrowserSearchSettings", empty_args);
}

}  // namespace

}  // namespace ash::settings
