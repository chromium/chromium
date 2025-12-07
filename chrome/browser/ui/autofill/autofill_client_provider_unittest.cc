// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_client_provider.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autofill/android/android_autofill_availability_status.h"
#include "chrome/browser/ui/autofill/autofill_client_provider_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/shared_preferences/shared_preferences_manager.h"
#include "chrome/browser/autofill/android/jni_test_headers/AutofillClientProviderTestUtils_jni.h"
#include "chrome/browser/preferences/android/chrome_shared_preferences.h"
#endif  // BUILDFLAG(IS_ANDROID)
namespace autofill {
namespace {

#if BUILDFLAG(IS_ANDROID)
constexpr const char* kAutofillThirdPartyModeState =
    "Chrome.AutofillThirdPartyMode.State";
constexpr const char* kAvailabilityMetric =
    "Autofill.AndroidAutofillAvailabilityStatus";
constexpr const char* kResetPrefMetric = "Autofill.ResetAutofillPrefToChrome";

// This allows to set the availability. Use to prevent compatibility checks that
// affect bots unpredictably.
void SetAutofillAvailabilityForTesting(
    AndroidAutofillAvailabilityStatus availability) {
  test::Java_AutofillClientProviderTestUtils_setAutofillAvailability(
      base::android::AttachCurrentThread(),
      jni_zero::ToJniType<int32_t>(base::android::AttachCurrentThread(),
                                   static_cast<int>(availability)));
}

void ResetAutofillAvailability() {
  test::Java_AutofillClientProviderTestUtils_resetAutofillAvailability(
      base::android::AttachCurrentThread());
}

#endif  // BUILDFLAG(IS_ANDROID)

class AutofillClientProviderBaseTest : public testing::Test {
 public:
  void SetUp() override { profile_ = std::make_unique<TestingProfile>(); }

  void TearDown() override {
    profile_.reset();  // Important since it also resets the prefs.
#if BUILDFLAG(IS_ANDROID)
    ResetAutofillAvailability();
#endif  // BUILDFLAG(IS_ANDROID)
  }

  TestingProfile* profile() { return profile_.get(); }

  AutofillClientProvider& provider() {
    return AutofillClientProviderFactory::GetForProfile(profile());
  }

  PrefService& prefs() { return *profile()->GetPrefs(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(AutofillClientProviderBaseTest, ProvidesServiceInNonIncognito) {
  AutofillClientProviderFactory::GetForProfile(profile());
}

TEST_F(AutofillClientProviderBaseTest, ProvidesServiceInIncognito) {
  AutofillClientProviderFactory::GetForProfile(
      profile()->GetOffTheRecordProfile(
          Profile::OTRProfileID::CreateUniqueForTesting(), true));
}

TEST_F(AutofillClientProviderBaseTest, ProvidesNoServiceWithoutProfile) {
  ASSERT_DEATH(AutofillClientProviderFactory::GetForProfile(nullptr), "");
}

TEST_F(AutofillClientProviderBaseTest, UsesBuiltInAutofillForDisabledPref) {
#if BUILDFLAG(IS_ANDROID)
  // Independent of platform or feature, a disabled pref means Chrome fills.
  prefs().SetBoolean(prefs::kAutofillUsingVirtualViewStructure, false);
#endif  // BUILDFLAG(IS_ANDROID)
  EXPECT_FALSE(provider().uses_platform_autofill());
}

#if BUILDFLAG(IS_ANDROID)
class AutofillClientProviderTest : public AutofillClientProviderBaseTest {
 public:
  AutofillClientProviderTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kAutofillThirdPartyModeContentProvider,
         features::kAutofillDeepLinkAutofillOptions},
        {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(AutofillClientProviderTest,
       CreateAndroidClientForEnabledPlatformAutofillSetting) {
  SetAutofillAvailabilityForTesting(
      AndroidAutofillAvailabilityStatus::kAvailable);
  EXPECT_TRUE(provider().uses_platform_autofill());
  EXPECT_TRUE(prefs().GetBoolean(prefs::kAutofillUsingVirtualViewStructure));
}

TEST_F(AutofillClientProviderTest,
       CreateChromeClientForDisablePlatformAutofillSetting) {
  SetAutofillAvailabilityForTesting(
      AndroidAutofillAvailabilityStatus::kSettingTurnedOff);
  EXPECT_FALSE(provider().uses_platform_autofill());
  EXPECT_FALSE(prefs().GetBoolean(prefs::kAutofillUsingVirtualViewStructure));
}

TEST_F(AutofillClientProviderTest, CreateChromeClientForDisabledPref) {
  base::HistogramTester histogram_tester;
  prefs().SetBoolean(prefs::kAutofillUsingVirtualViewStructure, false);
  EXPECT_FALSE(provider().uses_platform_autofill());

  // A changing pref doesn't change the clients for new tabs:
  prefs().SetBoolean(prefs::kAutofillUsingVirtualViewStructure, true);
  EXPECT_FALSE(provider().uses_platform_autofill());

  // The pref is used as is and not reset.
  histogram_tester.ExpectUniqueSample(kResetPrefMetric, false, 1);
}

TEST_F(AutofillClientProviderTest, CreateChromeClientIfPolicyPrefIsDisabled) {
  base::HistogramTester histogram_tester;
  prefs().SetBoolean(prefs::kAutofillThirdPartyPasswordManagersAllowed, false);

  // The general pref may be set but it's ineffective if a policy overrides it.
  prefs().SetBoolean(prefs::kAutofillUsingVirtualViewStructure, true);

  EXPECT_FALSE(provider().uses_platform_autofill());
  histogram_tester.ExpectUniqueSample(kResetPrefMetric, true, 1);

  // Check that the pref was reset.
  EXPECT_FALSE(prefs().GetBoolean(prefs::kAutofillUsingVirtualViewStructure));
  histogram_tester.ExpectUniqueSample(
      kAvailabilityMetric,
      AndroidAutofillAvailabilityStatus::kNotAllowedByPolicy, 1);
}

TEST_F(AutofillClientProviderTest,
       Propagates3PSettingForEnabledPlatformAutofillSetting) {
  SetAutofillAvailabilityForTesting(
      AndroidAutofillAvailabilityStatus::kAvailable);
  EXPECT_TRUE(provider().uses_platform_autofill());
  EXPECT_TRUE(prefs().GetBoolean(prefs::kAutofillUsingVirtualViewStructure));
  base::android::SharedPreferencesManager prefs =
      android::shared_preferences::GetChromeSharedPreferences();
  ASSERT_TRUE(prefs.ContainsKey(kAutofillThirdPartyModeState));
  EXPECT_TRUE(prefs.ReadBoolean(kAutofillThirdPartyModeState, false));
}

TEST_F(AutofillClientProviderTest,
       Propagates3PSettingForDisabledPlatformAutofillSetting) {
  SetAutofillAvailabilityForTesting(
      AndroidAutofillAvailabilityStatus::kSettingTurnedOff);
  EXPECT_FALSE(provider().uses_platform_autofill());
  EXPECT_FALSE(prefs().GetBoolean(prefs::kAutofillUsingVirtualViewStructure));
  base::android::SharedPreferencesManager prefs =
      android::shared_preferences::GetChromeSharedPreferences();
  ASSERT_TRUE(prefs.ContainsKey(kAutofillThirdPartyModeState));
  EXPECT_FALSE(prefs.ReadBoolean(kAutofillThirdPartyModeState, false));
}

#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace
}  // namespace autofill

#if BUILDFLAG(IS_ANDROID)
DEFINE_JNI(AutofillClientProviderTestUtils)
#endif
