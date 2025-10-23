// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/reset_settings_handler.h"

#include <stddef.h>

#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/google/google_brand.h"
#include "chrome/browser/profile_resetter/fake_profile_resetter.h"
#include "chrome/browser/profile_resetter/profile_resetter.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/default_search_manager.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "extensions/browser/pref_names.h"
#include "services/preferences/public/cpp/tracked/pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

using settings::ResetSettingsHandler;

namespace {

class TestingResetSettingsHandler : public ResetSettingsHandler {
 public:
  TestingResetSettingsHandler(TestingProfile* profile, content::WebUI* web_ui)
      : ResetSettingsHandler(profile), resetter_(profile) {
    set_web_ui(web_ui);
  }

  TestingResetSettingsHandler(const TestingResetSettingsHandler&) = delete;
  TestingResetSettingsHandler& operator=(const TestingResetSettingsHandler&) =
      delete;

  size_t Resets() const { return resetter_.Resets(); }

  using settings::ResetSettingsHandler::HandleResetProfileSettings;

 protected:
  ProfileResetter* GetResetter() override { return &resetter_; }

 private:
  FakeProfileResetter resetter_;
};

class ResetSettingsHandlerTest : public testing::Test {
 public:
  ResetSettingsHandlerTest() {
    google_brand::BrandForTesting brand_for_testing("");
    handler_ =
        std::make_unique<TestingResetSettingsHandler>(&profile_, &web_ui_);
  }

  TestingResetSettingsHandler* handler() { return handler_.get(); }
  content::TestWebUI* web_ui() { return &web_ui_; }
  TestingProfile* profile() { return &profile_; }

 private:
  // The order here matters.
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  content::TestWebUI web_ui_;
  std::unique_ptr<TestingResetSettingsHandler> handler_;
};

TEST_F(ResetSettingsHandlerTest, HandleResetProfileSettings) {
  base::Value::List list;
  std::string expected_callback_id("dummyCallbackId");
  list.Append(expected_callback_id);
  list.Append(false);
  list.Append("");
  handler()->HandleResetProfileSettings(list);
  // Check that the delegate ProfileResetter was called.
  EXPECT_EQ(1u, handler()->Resets());
  // Check that Javascript side is notified after resetting is done.
  EXPECT_EQ("cr.webUIResponse", web_ui()->call_data()[0]->function_name());
  const std::string* callback_id =
      web_ui()->call_data()[0]->arg1()->GetIfString();
  EXPECT_NE(nullptr, callback_id);
  EXPECT_EQ(expected_callback_id, *callback_id);
}

class ResetSettingsHandlerV2Test : public ResetSettingsHandlerTest {
 public:
  ResetSettingsHandlerV2Test() {
    feature_list_.InitAndEnableFeature(features::kShowResetProfileBannerV2);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(ResetSettingsHandlerV2Test, HandleGetTamperedPreferencePaths) {
  base::Value::List tampered_prefs;
  tampered_prefs.Append("some.unrelated.pref.path");
  tampered_prefs.Append(
      DefaultSearchManager::kDefaultSearchProviderDataPrefName);
  tampered_prefs.Append(prefs::kShowHomeButton);
  tampered_prefs.Append(prefs::kHomePage);
  tampered_prefs.Append(prefs::kPinnedTabs);
  tampered_prefs.Append("extensions.settings.EXTENSIONTESTID789");
  profile()->GetPrefs()->SetList(user_prefs::kTrackedPreferencesReset,
                                 std::move(tampered_prefs));

  base::Value::List args;
  args.Append("test-callback-id-123");
  handler()->HandleGetTamperedPreferencePaths(args);

  const content::TestWebUI::CallData& call_data = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIResponse", call_data.function_name());
  ASSERT_EQ("test-callback-id-123", call_data.arg1()->GetString());
  ASSERT_TRUE(call_data.arg2()->GetBool());

  const base::Value::List* result_list = call_data.arg3()->GetIfList();
  ASSERT_TRUE(result_list);

  ASSERT_EQ(5U, result_list->size());

  std::set<std::string> results;
  for (const auto& value : *result_list) {
    results.insert(value.GetString());
  }

  EXPECT_TRUE(results.count(l10n_util::GetStringUTF8(IDS_SETTINGS_RESET_DSE)));
  EXPECT_TRUE(
      results.count(l10n_util::GetStringUTF8(IDS_SETTINGS_SHOW_HOME_BUTTON)));
  EXPECT_TRUE(
      results.count(l10n_util::GetStringUTF8(IDS_SETTINGS_RESET_HOMEPAGE)));
  EXPECT_TRUE(
      results.count(l10n_util::GetStringUTF8(IDS_SETTINGS_RESET_PINNED_TABS)));
  EXPECT_TRUE(
      results.count(l10n_util::GetStringUTF8(IDS_SETTINGS_RESET_EXTENSIONS)));
}

TEST_F(ResetSettingsHandlerV2Test,
       HandleGetTamperedPreferencePaths_EmptyWhenNoPrefs) {
  profile()->GetPrefs()->ClearPref(user_prefs::kTrackedPreferencesReset);

  base::Value::List args;
  args.Append("test-callback-id-456");
  handler()->HandleGetTamperedPreferencePaths(args);

  const content::TestWebUI::CallData& call_data = *web_ui()->call_data().back();
  const base::Value::List* result_list = call_data.arg3()->GetIfList();
  ASSERT_TRUE(result_list);
  EXPECT_TRUE(result_list->empty());
}

TEST_F(ResetSettingsHandlerTest,
       HandleGetTamperedPreferencePaths_EmptyWhenFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kShowResetProfileBannerV2);
  // Setup: Add a tampered pref.
  base::Value::List tampered_prefs;
  tampered_prefs.Append(prefs::kShowHomeButton);
  profile()->GetPrefs()->SetList(user_prefs::kTrackedPreferencesReset,
                                 std::move(tampered_prefs));

  base::Value::List args;
  args.Append("test-callback-id-789");
  handler()->HandleGetTamperedPreferencePaths(args);

  const content::TestWebUI::CallData& call_data = *web_ui()->call_data().back();
  const base::Value::List* result_list = call_data.arg3()->GetIfList();
  ASSERT_TRUE(result_list);
  EXPECT_TRUE(result_list->empty());
}

}  // namespace
