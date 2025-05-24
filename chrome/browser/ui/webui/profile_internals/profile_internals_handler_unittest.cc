// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/profile_internals/profile_internals_handler.h"

#include <memory>

#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class ProfileInternalsHandlerTest : public testing::Test {
 public:
  ProfileInternalsHandlerTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override { ASSERT_TRUE(profile_manager_.SetUp()); }

  void TearDown() override { web_ui_.ClearTrackedCalls(); }

  content::TestWebUI& ConfigureWebUI(Profile* web_ui_profile) {
    handler_ = std::make_unique<ProfileInternalsHandler>();
    web_ui_.set_web_contents(
        web_contents_factory_.CreateWebContents(web_ui_profile));
    handler_->set_web_ui(&web_ui_);
    handler_->RegisterMessages();
    return web_ui_;
  }

  Profile* CreateTestingProfile(const std::string& profile_name) {
    auto* profile = profile_manager_.CreateTestingProfile(
        profile_name,
        {
            TestingProfile::TestingFactory(
                TemplateURLServiceFactory::GetInstance(),
                base::BindRepeating(
                    &TemplateURLServiceFactory::BuildInstanceFor)),
        });
    CHECK(profile);
    return profile;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  content::TestWebContentsFactory web_contents_factory_;
  content::TestWebUI web_ui_;
  std::unique_ptr<ProfileInternalsHandler> handler_;
};

TEST_F(ProfileInternalsHandlerTest, DebugDataLoaded) {
  Profile* web_ui_profile = CreateTestingProfile("A");
  CreateTestingProfile("B");
  content::TestWebUI& web_ui = ConfigureWebUI(web_ui_profile);

  {
    base::Value::List empty_args;
    web_ui.HandleReceivedMessage("getProfilesList", empty_args);
  }

  ASSERT_TRUE(!web_ui.call_data().empty());
  const content::TestWebUI::CallData& call_data = *web_ui.call_data().back();
  ASSERT_EQ("cr.webUIListenerCallback", call_data.function_name());
  ASSERT_EQ("profiles-list-changed", call_data.arg1()->GetString());
  ASSERT_TRUE(call_data.arg2()->is_list());
  const base::Value::List& pushed_list = call_data.arg2()->GetList();

  EXPECT_EQ(2u, pushed_list.size());
}
