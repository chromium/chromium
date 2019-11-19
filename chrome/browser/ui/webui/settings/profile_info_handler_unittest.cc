// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/profile_info_handler.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "net/base/data_url.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "url/gurl.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#endif

namespace settings {

namespace {

#if defined(OS_CHROMEOS)
constexpr char fake_id[] = "fake_id";
constexpr char fake_email[] = "fake_id@gmail.com";
#endif

class TestProfileInfoHandler : public ProfileInfoHandler {
 public:
  explicit TestProfileInfoHandler(Profile* profile)
      : ProfileInfoHandler(profile) {}

  using ProfileInfoHandler::set_web_ui;
};

}  // namespace

class ProfileInfoHandlerTest : public testing::Test {
 public:
  ProfileInfoHandlerTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()),
        profile_(nullptr) {}

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());

#if defined(OS_CHROMEOS)
    chromeos::FakeChromeUserManager* fake_user_manager =
        new chromeos::FakeChromeUserManager;
    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        base::WrapUnique(fake_user_manager));
    profile_ = profile_manager_.CreateTestingProfile(fake_email);
    fake_user_manager->AddUser(AccountId::FromUserEmail(fake_email));
#else
    profile_ = profile_manager_.CreateTestingProfile("Profile 1");
#endif

    handler_ = std::make_unique<TestProfileInfoHandler>(profile_);
    handler_->set_web_ui(&web_ui_);
  }

  void VerifyProfileInfo(const base::Value* call_argument) {
    const base::DictionaryValue* response = nullptr;
    ASSERT_TRUE(call_argument->GetAsDictionary(&response));

    std::string name;
    std::string icon_url;
    ASSERT_TRUE(response->GetString("name", &name));
    ASSERT_TRUE(response->GetString("iconUrl", &icon_url));

#if defined(OS_CHROMEOS)
    EXPECT_EQ(fake_id, name);
    EXPECT_FALSE(icon_url.empty());
#else
    EXPECT_EQ("Profile 1", name);

    std::string mime, charset, data;
    EXPECT_TRUE(net::DataURL::Parse(GURL(icon_url), &mime, &charset, &data));

    EXPECT_EQ("image/png", mime);
    SkBitmap bitmap;
    EXPECT_TRUE(gfx::PNGCodec::Decode(
        reinterpret_cast<const unsigned char*>(data.data()), data.size(),
        &bitmap));
#endif
  }

  content::TestWebUI* web_ui() { return &web_ui_; }
  Profile* profile() const { return profile_; }
  TestProfileInfoHandler* handler() const { return handler_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  content::TestWebUI web_ui_;

#if defined(OS_CHROMEOS)
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;
#endif

  Profile* profile_;
  std::unique_ptr<TestProfileInfoHandler> handler_;
};

TEST_F(ProfileInfoHandlerTest, GetProfileInfo) {
  base::ListValue list_args;
  list_args.AppendString("get-profile-info-callback-id");
  handler()->HandleGetProfileInfo(&list_args);

  EXPECT_EQ(1U, web_ui()->call_data().size());

  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIResponse", data.function_name());

  std::string callback_id;
  ASSERT_TRUE(data.arg1()->GetAsString(&callback_id));
  EXPECT_EQ("get-profile-info-callback-id", callback_id);

  bool success = false;
  ASSERT_TRUE(data.arg2()->GetAsBoolean(&success));
  EXPECT_TRUE(success);

  VerifyProfileInfo(data.arg3());
}

TEST_F(ProfileInfoHandlerTest, PushProfileInfo) {
  handler()->AllowJavascript();

  handler()->OnProfileAvatarChanged(base::FilePath());

  EXPECT_EQ(1U, web_ui()->call_data().size());

  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIListenerCallback", data.function_name());

  std::string event_id;
  ASSERT_TRUE(data.arg1()->GetAsString(&event_id));
  EXPECT_EQ(ProfileInfoHandler::kProfileInfoChangedEventName, event_id);

  VerifyProfileInfo(data.arg2());
}

}  // namespace settings
