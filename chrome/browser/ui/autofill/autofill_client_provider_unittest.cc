// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_client_provider.h"

#include <memory>

#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/autofill/autofill_client_provider_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "components/autofill/content/browser/test_content_autofill_client.h"

namespace autofill {
namespace {

class AutofillClientProviderBaseTest : public testing::Test {
 public:
  void SetUp() override { profile_ = std::make_unique<TestingProfile>(); }

  void TearDown() override {
    profile_.reset();  // Important since it also resets the prefs.
  }

  TestingProfile* profile() { return profile_.get(); }

  AutofillClientProvider& provider() {
    return AutofillClientProviderFactory::GetForProfile(profile());
  }

  PrefService* prefs() { return profile()->GetPrefs(); }

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
  prefs()->SetBoolean(prefs::kAutofillUsingVirtualViewStructure, false);
#endif  // BUILDFLAG(IS_ANDROID)
  EXPECT_FALSE(provider().uses_platform_autofill());
}

#if BUILDFLAG(IS_ANDROID)
class AutofillClientProviderLegacyTest : public AutofillClientProviderBaseTest {
 public:
  void SetUp() override {
    AutofillClientProviderBaseTest::SetUp();
    scoped_feature_list_.InitAndDisableFeature(
        features::kAutofillVirtualViewStructureAndroid);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(AutofillClientProviderLegacyTest, AlwaysCreatesChromeClient) {
  // The pref is irrelevant if the feature is disabled.
  prefs()->SetBoolean(prefs::kAutofillUsingVirtualViewStructure, true);
  EXPECT_FALSE(provider().uses_platform_autofill());
}

class AutofillClientProviderTest : public AutofillClientProviderBaseTest {
 public:
  AutofillClientProviderTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kAutofillVirtualViewStructureAndroid,
        {{features::kAutofillVirtualViewStructureAndroidSkipsCompatibilityCheck
              .name,
          "true"}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(AutofillClientProviderTest, CreateAndroidClientForEnabledPref) {
  prefs()->SetBoolean(prefs::kAutofillUsingVirtualViewStructure, true);
  EXPECT_TRUE(provider().uses_platform_autofill());

  // A changing pref doesn't change the clients for new tabs:
  prefs()->SetBoolean(prefs::kAutofillUsingVirtualViewStructure, false);
  EXPECT_TRUE(provider().uses_platform_autofill());
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace
}  // namespace autofill
