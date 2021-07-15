// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/privacy_sandbox_handler.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/federated_learning/floc_id_provider_factory.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/federated_learning/floc_id.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

constexpr char kCallbackId[] = "test-callback-id";

class MockPrivacySandboxObserver : public PrivacySandboxSettings::Observer {
 public:
  MOCK_METHOD1(OnFlocDataAccessibleSinceUpdated, void(bool));
};

// Confirms that the |floc_id| dictionary provided matches the current FLoC
// information for |profile|.
void ValidateFlocId(const base::Value* floc_id, Profile* profile) {
  auto* privacy_sandbox_settings =
      PrivacySandboxSettingsFactory::GetForProfile(profile);
  auto* floc_id_provider =
      federated_learning::FlocIdProviderFactory::GetForProfile(profile);

  ASSERT_TRUE(floc_id->is_dict());
  EXPECT_EQ(
      base::UTF16ToUTF8(privacy_sandbox_settings->GetFlocStatusForDisplay()),
      *floc_id->FindStringPath("trialStatus"));
  EXPECT_EQ(base::UTF16ToUTF8(privacy_sandbox_settings->GetFlocIdForDisplay()),
            *floc_id->FindStringPath("cohort"));
  EXPECT_EQ(
      base::UTF16ToUTF8(PrivacySandboxSettings::GetFlocIdNextUpdateForDisplay(
          floc_id_provider, profile->GetPrefs(), base::Time::Now())),
      *floc_id->FindStringPath("nextUpdate"));
  EXPECT_EQ(privacy_sandbox_settings->IsFlocIdResettable(),
            floc_id->FindBoolPath("canReset"));
}

}  // namespace

namespace settings {

class PrivacySandboxHandlerTest : public testing::Test {
 public:
  PrivacySandboxHandlerTest()
      : browser_task_environment_(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    web_ui_ = std::make_unique<content::TestWebUI>();
    web_ui_->set_web_contents(web_contents_.get());
    handler_ = std::make_unique<PrivacySandboxHandler>();
    handler_->set_web_ui(web_ui());
    handler_->AllowJavascript();
    web_ui_->ClearTrackedCalls();
  }

  void TearDown() override {
    handler_->set_web_ui(nullptr);
    handler_.reset();
    web_ui_.reset();
  }

  content::TestWebUI* web_ui() { return web_ui_.get(); }
  PrivacySandboxHandler* handler() { return handler_.get(); }
  TestingProfile* profile() { return &profile_; }
  PrivacySandboxSettings* privacy_sandbox_settings() {
    return PrivacySandboxSettingsFactory::GetForProfile(profile());
  }

 private:
  content::BrowserTaskEnvironment browser_task_environment_;
  TestingProfile profile_;
  std::unique_ptr<content::WebContents> web_contents_ =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  std::unique_ptr<content::TestWebUI> web_ui_;
  std::unique_ptr<PrivacySandboxHandler> handler_;
};

TEST_F(PrivacySandboxHandlerTest, GetFlocId) {
  federated_learning::FlocId floc_id = federated_learning::FlocId::CreateValid(
      123456, base::Time(), base::Time::Now(),
      /*sorting_lsh_version=*/0);
  floc_id.SaveToPrefs(profile()->GetTestingPrefService());

  base::Value args(base::Value::Type::LIST);
  args.Append(kCallbackId);
  handler()->HandleGetFlocId(&base::Value::AsListValue(args));

  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  EXPECT_EQ(kCallbackId, data.arg1()->GetString());
  EXPECT_EQ("cr.webUIResponse", data.function_name());
  ASSERT_TRUE(data.arg2()->GetBool());
  ValidateFlocId(data.arg3(), profile());
}

TEST_F(PrivacySandboxHandlerTest, ResetFlocId) {
  federated_learning::FlocId floc_id = federated_learning::FlocId::CreateValid(
      123456, base::Time(), base::Time::Now(),
      /*sorting_lsh_version=*/0);
  floc_id.SaveToPrefs(profile()->GetTestingPrefService());

  // Observers of the PrivacySandboxSettings service should be informed that
  // the FLoC ID was reset.
  MockPrivacySandboxObserver observer;
  privacy_sandbox_settings()->AddObserver(&observer);
  EXPECT_CALL(observer, OnFlocDataAccessibleSinceUpdated(true));

  base::Value args(base::Value::Type::LIST);
  handler()->HandleResetFlocId(&base::Value::AsListValue(args));

  // Resetting the FLoC ID should also fire the appropriate WebUI listener.
  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIListenerCallback", data.function_name());
  EXPECT_EQ("floc-id-changed", data.arg1()->GetString());
  ValidateFlocId(data.arg2(), profile());
}

}  // namespace settings
