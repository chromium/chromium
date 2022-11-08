// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/on_startup_handler.h"

#include <string>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "build/build_config.h"
#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#endif
#include "build/chromeos_buildflags.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kCallbackId[] = "test-on-startup-callback-id";

class TestOnStartupHandler : public settings::OnStartupHandler {
 public:
  explicit TestOnStartupHandler(Profile* profile)
      : settings::OnStartupHandler(profile) {}

  using settings::OnStartupHandler::set_web_ui;
};

}  // namespace

namespace settings {

class OnStartupHandlerTest : public testing::Test {
 public:
  OnStartupHandlerTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()),
        profile_(nullptr) {}

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());

#if BUILDFLAG(IS_CHROMEOS_ASH)
    auto* fake_user_manager = new ash::FakeChromeUserManager;
    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        base::WrapUnique(fake_user_manager));
    constexpr char kFakeEmail[] = "fake_id@gmail.com";
    profile_ = profile_manager_.CreateTestingProfile(kFakeEmail);
    fake_user_manager->AddUser(AccountId::FromUserEmail(kFakeEmail));
#else
    profile_ = profile_manager_.CreateTestingProfile("Profile 1");
#endif

    handler_ = std::make_unique<TestOnStartupHandler>(profile_);
    handler_->set_web_ui(&web_ui_);
  }

  TestOnStartupHandler* handler() { return handler_.get(); }
  Profile* profile() const { return profile_; }
  content::TestWebUI* web_ui() { return &web_ui_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  std::unique_ptr<TestOnStartupHandler> handler_;
  raw_ptr<Profile> profile_;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;
#endif
  content::TestWebUI web_ui_;
};

TEST_F(OnStartupHandlerTest, HandleGetNtpExtension) {
  base::Value::List list_args;
  list_args.Append(kCallbackId);
  handler()->HandleGetNtpExtension(list_args);

  EXPECT_EQ(1U, web_ui()->call_data().size());

  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIResponse", data.function_name());

  ASSERT_TRUE(data.arg1()->is_string());
  EXPECT_EQ(kCallbackId, data.arg1()->GetString());

  ASSERT_TRUE(data.arg2()->is_bool());
  EXPECT_TRUE(data.arg2()->GetBool());
}

TEST_F(OnStartupHandlerTest, HandleValidateStartupPage_Valid) {
  base::Value::List list_args;
  list_args.Append(kCallbackId);
  list_args.Append("http://example.com");
  handler()->HandleValidateStartupPage(list_args);

  EXPECT_EQ(1U, web_ui()->call_data().size());

  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIResponse", data.function_name());

  ASSERT_TRUE(data.arg1()->is_string());
  EXPECT_EQ(kCallbackId, data.arg1()->GetString());

  ASSERT_TRUE(data.arg2()->is_bool());
  EXPECT_TRUE(data.arg2()->GetBool());

  ASSERT_TRUE(data.arg3()->is_bool());
  EXPECT_TRUE(data.arg3()->GetBool());
}

TEST_F(OnStartupHandlerTest, HandleValidateStartupPage_Invalid) {
  base::Value::List list_args;
  list_args.Append(kCallbackId);
  list_args.Append("@");
  handler()->HandleValidateStartupPage(list_args);

  EXPECT_EQ(1U, web_ui()->call_data().size());

  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIResponse", data.function_name());

  ASSERT_TRUE(data.arg1()->is_string());
  EXPECT_EQ(kCallbackId, data.arg1()->GetString());

  ASSERT_TRUE(data.arg2()->is_bool());
  EXPECT_TRUE(data.arg2()->GetBool());

  ASSERT_TRUE(data.arg3()->is_bool());
  EXPECT_FALSE(data.arg3()->GetBool());
}

}  // namespace settings
