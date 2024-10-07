// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/search_engine_choice/search_engine_choice_service.h"

#include <memory>
#include <string_view>
#include <vector>

#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/version.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/country_codes/country_codes.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/policy/core/common/mock_policy_service.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/search_engines/default_search_manager.h"
#include "components/search_engines/eea_countries_ids.h"
#include "components/search_engines/prepopulated_engines.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_metrics_service_accessor.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_utils.h"
#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_data_util.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/version_info/version_info.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "components/search_engines/android/test_utils_jni_headers/SearchEngineChoiceServiceTestUtil_jni.h"
#endif

using search_engines::RepromptResult;
using search_engines::WipeSearchEngineChoiceReason;
using ::testing::NiceMock;

namespace search_engines {
namespace {
#if BUILDFLAG(IS_ANDROID)
class TestSupportAndroid {
 public:
  TestSupportAndroid() {
    JNIEnv* env = base::android::AttachCurrentThread();
    base::android::ScopedJavaLocalRef<jobject> java_ref =
        Java_SearchEngineChoiceServiceTestUtil_Constructor(env);
    java_test_util_ref_.Reset(env, java_ref.obj());
  }

  ~TestSupportAndroid() {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_SearchEngineChoiceServiceTestUtil_destroy(env, java_test_util_ref_);
  }

  void ReturnDeviceCountry(const std::string& device_country) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_SearchEngineChoiceServiceTestUtil_returnDeviceCountry(
        env, java_test_util_ref_,
        base::android::ConvertUTF8ToJavaString(env, device_country));
  }

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_test_util_ref_;
};
#endif

const int kBelgiumCountryId = country_codes::CountryCharsToCountryID('B', 'E');

}  // namespace

class SearchEngineChoiceServiceTest : public ::testing::Test {
 public:
  SearchEngineChoiceServiceTest() {
    TemplateURLService::RegisterProfilePrefs(pref_service_.registry());
    DefaultSearchManager::RegisterProfilePrefs(pref_service_.registry());
    TemplateURLPrepopulateData::RegisterProfilePrefs(pref_service_.registry());
    local_state_.registry()->RegisterBooleanPref(
        metrics::prefs::kMetricsReportingEnabled, true);
    local_state_.registry()->RegisterInt64Pref(
        prefs::kDefaultSearchProviderGuestModePrepopulatedId, 0);

    // Override the country checks to simulate being in Belgium.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kSearchEngineChoiceCountry, "BE");

    // Metrics reporting is disabled for non-branded builds.
    SearchEngineChoiceMetricsServiceAccessor::
        SetForceIsMetricsReportingEnabledPrefLookup(true);

    InitMockPolicyService();
    CheckPoliciesInitialState();
  }

  ~SearchEngineChoiceServiceTest() override = default;

  void InitService(int variation_country_id = country_codes::kCountryIDUnknown,
                   bool force_reset = false,
                   bool is_profile_eligible_for_dse_guest_propagation = false) {
    if (!force_reset) {
      // If something refers to the existing instance, expect to run into
      // issues!
      CHECK(!search_engine_choice_service_);
    }
    search_engine_choice_service_ = std::make_unique<SearchEngineChoiceService>(
        pref_service_, &local_state_,
        is_profile_eligible_for_dse_guest_propagation, variation_country_id);
  }

  policy::MockPolicyService& policy_service() { return policy_service_; }
  policy::PolicyMap& policy_map() { return policy_map_; }
  sync_preferences::TestingPrefServiceSyncable* pref_service() {
    return &pref_service_;
  }
  TemplateURLService& template_url_service() {
    if (!template_url_service_) {
      template_url_service_ = std::make_unique<TemplateURLService>(
          pref_service_, search_engine_choice_service());
    }

    return CHECK_DEREF(template_url_service_.get());
  }
  search_engines::SearchEngineChoiceService& search_engine_choice_service() {
    if (!search_engine_choice_service_) {
      InitService();
    }

    return CHECK_DEREF(search_engine_choice_service_.get());
  }
  TestingPrefServiceSimple& local_state() { return local_state_; }

  base::HistogramTester histogram_tester_;

 private:
  void InitMockPolicyService() {
    ON_CALL(policy_service_, GetPolicies(::testing::Eq(policy::PolicyNamespace(
                                 policy::POLICY_DOMAIN_CHROME, std::string()))))
        .WillByDefault(::testing::ReturnRef(policy_map_));
  }

  // Test that the `DefaultSearchProviderEnabled` and
  // `DefaultSearchProviderSearchURL` policies are not initially set.
  void CheckPoliciesInitialState() {
    const auto& policies = policy_service().GetPolicies(
        policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, std::string()));

    const auto* default_search_provider_enabled = policies.GetValue(
        policy::key::kDefaultSearchProviderEnabled, base::Value::Type::BOOLEAN);
    const auto* default_search_provider_search_url =
        policies.GetValue(policy::key::kDefaultSearchProviderSearchURL,
                          base::Value::Type::STRING);

    ASSERT_FALSE(default_search_provider_enabled);
    ASSERT_FALSE(default_search_provider_search_url);
  }

  sync_preferences::TestingPrefServiceSyncable pref_service_;
  TestingPrefServiceSimple local_state_;
  std::unique_ptr<search_engines::SearchEngineChoiceService>
      search_engine_choice_service_;
  NiceMock<policy::MockPolicyService> policy_service_;
  policy::PolicyMap policy_map_;
  std::unique_ptr<TemplateURLService> template_url_service_;
};

// Test that the choice screen doesn't get displayed if the profile is not
// regular.
TEST_F(SearchEngineChoiceServiceTest,
       DoNotShowChoiceScreenWithNotRegularProfile) {
  EXPECT_EQ(search_engine_choice_service().GetStaticChoiceScreenConditions(
                policy_service(), /*is_regular_profile=*/false,
                template_url_service()),
            SearchEngineChoiceScreenConditions::kUnsupportedBrowserType);
}

// Test that the choice screen gets displayed if the
// `DefaultSearchProviderEnabled` policy is not set.
TEST_F(SearchEngineChoiceServiceTest, ShowChoiceScreenIfPoliciesAreNotSet) {
  SearchEngineChoiceScreenConditions expected_choice_screen_condition =
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA) || \
    BUILDFLAG(CHROME_FOR_TESTING)
      SearchEngineChoiceScreenConditions::kUnsupportedBrowserType;
#else
      SearchEngineChoiceScreenConditions::kEligible;
#endif

  EXPECT_EQ(search_engine_choice_service().GetStaticChoiceScreenConditions(
                policy_service(), /*is_regular_profile=*/true,
                template_url_service()),
            expected_choice_screen_condition);
  EXPECT_EQ(search_engine_choice_service().GetDynamicChoiceScreenConditions(
                template_url_service()),
            expected_choice_screen_condition);
}

// Test that the choice screen does not get displayed if the provider list is
// overridden in the intial_preferences file.
TEST_F(SearchEngineChoiceServiceTest,
       DoNotShowChoiceScreenWithProviderListOverride) {
  base::Value::List override_list;
  pref_service()->SetList(prefs::kSearchProviderOverrides,
                          override_list.Clone());

  EXPECT_EQ(search_engine_choice_service().GetStaticChoiceScreenConditions(
                policy_service(), /*is_regular_profile=*/true,
                template_url_service()),
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA) || \
    BUILDFLAG(CHROME_FOR_TESTING)
            SearchEngineChoiceScreenConditions::kUnsupportedBrowserType
#else
            SearchEngineChoiceScreenConditions::kSearchProviderOverride
#endif
  );
}

// Test that the choice screen doesn't get displayed if the
// 'DefaultSearchProviderEnabled' policy is set to false.
TEST_F(SearchEngineChoiceServiceTest, DoNotShowChoiceScreenIfPolicySetToFalse) {
  policy_map().Set(policy::key::kDefaultSearchProviderEnabled,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD, base::Value(false), nullptr);

  base::Value::Dict dict;
  dict.Set(DefaultSearchManager::kDisabledByPolicy, true);
  pref_service()->SetManagedPref(
      DefaultSearchManager::kDefaultSearchProviderDataPrefName,
      std::move(dict));

  // Based on these policies, no DSE should be available.
  ASSERT_FALSE(template_url_service().GetDefaultSearchProvider());

  EXPECT_EQ(search_engine_choice_service().GetStaticChoiceScreenConditions(
                policy_service(), /*is_regular_profile=*/true,
                template_url_service()),
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA) || \
    BUILDFLAG(CHROME_FOR_TESTING)
            SearchEngineChoiceScreenConditions::kUnsupportedBrowserType
#else
            SearchEngineChoiceScreenConditions::kControlledByPolicy
#endif
  );
  EXPECT_EQ(search_engine_choice_service().GetDynamicChoiceScreenConditions(
                template_url_service()),
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA) || \
    BUILDFLAG(CHROME_FOR_TESTING)
            SearchEngineChoiceScreenConditions::kUnsupportedBrowserType
#else
            SearchEngineChoiceScreenConditions::kControlledByPolicy
#endif
  );
}

// Test that the choice screen gets displayed if the
// 'DefaultSearchProviderEnabled' policy is set to true but the
// 'DefaultSearchProviderSearchURL' policy is not set.
TEST_F(SearchEngineChoiceServiceTest,
       ShowChoiceScreenIfPolicySetToTrueWithoutUrlSet) {
  policy_map().Set(policy::key::kDefaultSearchProviderEnabled,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD, base::Value(true), nullptr);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA) || \
    BUILDFLAG(CHROME_FOR_TESTING)
  EXPECT_EQ(search_engine_choice_service().GetStaticChoiceScreenConditions(
                policy_service(), /*is_regular_profile=*/true,
                template_url_service()),
            SearchEngineChoiceScreenConditions::kUnsupportedBrowserType);
#else
  EXPECT_EQ(search_engine_choice_service().GetStaticChoiceScreenConditions(
                policy_service(), /*is_regular_profile=*/true,
                template_url_service()),
            SearchEngineChoiceScreenConditions::kEligible);
  EXPECT_EQ(search_engine_choice_service().GetDynamicChoiceScreenConditions(
                template_url_service()),
            SearchEngineChoiceScreenConditions::kEligible);
#endif
}

#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
TEST_F(SearchEngineChoiceServiceTest, GuestSessionDsePropagation) {
  base::test::ScopedFeatureList scoped_feature_list{
      switches::kSearchEngineChoiceGuestExperience};
  InitService(kBelgiumCountryId,
              /*=force_reset=*/true,
              /*is_profile_eligible_for_dse_guest_propagation=*/true);

  EXPECT_FALSE(local_state().HasPrefPath(
      prefs::kDefaultSearchProviderGuestModePrepopulatedId));
  EXPECT_FALSE(search_engine_choice_service()
                   .GetSavedSearchEngineBetweenGuestSessions()
                   .has_value());

  constexpr int prepopulated_id = 1;
  search_engine_choice_service().SetSavedSearchEngineBetweenGuestSessions(
      prepopulated_id);
  EXPECT_EQ(local_state().GetInt64(
                prefs::kDefaultSearchProviderGuestModePrepopulatedId),
            prepopulated_id);
  EXPECT_EQ(
      search_engine_choice_service().GetSavedSearchEngineBetweenGuestSessions(),
      prepopulated_id);

  // The guest DSE is not propagated to services that are not guest profiles.
  InitService(kBelgiumCountryId,
              /*=force_reset=*/true,
              /*is_profile_eligible_for_dse_guest_propagation=*/false);
  EXPECT_FALSE(search_engine_choice_service()
                   .GetSavedSearchEngineBetweenGuestSessions()
                   .has_value());

  // A new guest service propagates the DSE.
  InitService(country_codes::kCountryIDUnknown,
              /*=force_reset=*/true,
              /*is_profile_eligible_for_dse_guest_propagation=*/true);
  EXPECT_EQ(
      search_engine_choice_service().GetSavedSearchEngineBetweenGuestSessions(),
      prepopulated_id);
}

TEST_F(SearchEngineChoiceServiceTest,
       UpdatesDefaultSearchEngineManagerForGuestMode) {
  base::test::ScopedFeatureList scoped_feature_list{
      switches::kSearchEngineChoiceGuestExperience};
  InitService(kBelgiumCountryId,
              /*=force_reset=*/true,
              /*is_profile_eligible_for_dse_guest_propagation=*/true);

  DefaultSearchManager manager(pref_service(), &search_engine_choice_service(),
                               base::NullCallback());
  DefaultSearchManager::Source source;
  EXPECT_EQ(manager.GetDefaultSearchEngine(&source)->prepopulate_id, 1);
  EXPECT_EQ(source, DefaultSearchManager::Source::FROM_FALLBACK);

  // Test the changes in the SearchEngineChoiceService propagate to the DSE
  // manager.
  search_engine_choice_service().SetSavedSearchEngineBetweenGuestSessions(2);
  EXPECT_EQ(manager.GetDefaultSearchEngine(&source)->prepopulate_id, 2);
  EXPECT_EQ(source, DefaultSearchManager::Source::FROM_FALLBACK);
}
#endif
// Test that the choice screen doesn't get displayed if the
// 'DefaultSearchProviderEnabled' policy is set to true and the
// DefaultSearchProviderSearchURL' is set.
TEST_F(SearchEngineChoiceServiceTest,
       DoNotShowChoiceScreenIfPolicySetToTrueWithUrlSet) {
  policy_map().Set(policy::key::kDefaultSearchProviderEnabled,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD, base::Value(true), nullptr);
  policy_map().Set(policy::key::kDefaultSearchProviderSearchURL,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD, base::Value("test"), nullptr);

  TemplateURLData data_from_policies;
  data_from_policies.SetURL("test");
  base::Value::Dict dict = TemplateURLDataToDictionary(data_from_policies);
  dict.Set(DefaultSearchManager::kCreatedByPolicy,
           static_cast<int>(
               TemplateURLData::CreatedByPolicy::kDefaultSearchProvider));
  pref_service()->SetManagedPref(
      DefaultSearchManager::kDefaultSearchProviderDataPrefName,
      std::move(dict));

  ASSERT_TRUE(template_url_service().GetDefaultSearchProvider());
  ASSERT_EQ("test", template_url_service().GetDefaultSearchProvider()->url());

  EXPECT_EQ(search_engine_choice_service().GetStaticChoiceScreenConditions(
                policy_service(), /*is_regular_profile=*/true,
                template_url_service()),
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA) || \
    BUILDFLAG(CHROME_FOR_TESTING)
            SearchEngineChoiceScreenConditions::kUnsupportedBrowserType
#else
            SearchEngineChoiceScreenConditions::kControlledByPolicy
#endif
  );
  EXPECT_EQ(search_engine_choice_service().GetDynamicChoiceScreenConditions(
                template_url_service()),
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA) || \
    BUILDFLAG(CHROME_FOR_TESTING)
            SearchEngineChoiceScreenConditions::kUnsupportedBrowserType
#else
            SearchEngineChoiceScreenConditions::kControlledByPolicy
#endif
  );
}

// Test that the choice screen gets displayed if and only if the
// `kDefaultSearchProviderChoiceScreenTimestamp` pref is not set. Setting this
// pref means that the user has made a search engine choice in the choice
// screen.
TEST_F(SearchEngineChoiceServiceTest,
       ShowChoiceScreenIfTheTimestampPrefIsNotSet) {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA) || \
    BUILDFLAG(CHROME_FOR_TESTING)
  EXPECT_EQ(search_engine_choice_service().GetStaticChoiceScreenConditions(
                policy_service(), /*is_regular_profile=*/true,
                template_url_service()),
            SearchEngineChoiceScreenConditions::kUnsupportedBrowserType);
#else
  EXPECT_EQ(search_engine_choice_service().GetStaticChoiceScreenConditions(
                policy_service(), /*is_regular_profile=*/true,
                template_url_service()),
            SearchEngineChoiceScreenConditions::kEligible);
  EXPECT_EQ(search_engine_choice_service().GetDynamicChoiceScreenConditions(
                template_url_service()),
            SearchEngineChoiceScreenConditions::kEligible);
#endif

  search_engine_choice_service().RecordChoiceMade(
      search_engines::ChoiceMadeLocation::kChoiceScreen,
      &template_url_service());
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA) || \
    BUILDFLAG(CHROME_FOR_TESTING)
  EXPECT_EQ(search_engine_choice_service().GetStaticChoiceScreenConditions(
                policy_service(), /*is_regular_profile=*/true,
                template_url_service()),
            SearchEngineChoiceScreenConditions::kUnsupportedBrowserType);
#else
  EXPECT_EQ(search_engine_choice_service().GetStaticChoiceScreenConditions(
                policy_service(), /*is_regular_profile=*/true,
                template_url_service()),
            SearchEngineChoiceScreenConditions::kAlreadyCompleted);
  EXPECT_EQ(search_engine_choice_service().GetDynamicChoiceScreenConditions(
                template_url_service()),
            SearchEngineChoiceScreenConditions::kAlreadyCompleted);
#endif
}

// Test that there is a regional condition controlling eligibility.
TEST_F(SearchEngineChoiceServiceTest,
       DoNotShowChoiceScreenIfCountryOutOfScope) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kSearchEngineChoiceCountry, "US");
  EXPECT_EQ(search_engine_choice_service().GetStaticChoiceScreenConditions(
                policy_service(), /*is_regular_profile=*/true,
                template_url_service()),
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA) || \
    BUILDFLAG(CHROME_FOR_TESTING)
            SearchEngineChoiceScreenConditions::kUnsupportedBrowserType
#else
            SearchEngineChoiceScreenConditions::kNotInRegionalScope
#endif
  );
}

// Test that the choice screen does get displayed even if completed if the
// command line argument for forcing it is set.
TEST_F(SearchEngineChoiceServiceTest,
       ShowChoiceScreenWithForceCommandLineFlag) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kForceSearchEngineChoiceScreen);
  search_engines::MarkSearchEngineChoiceCompletedForTesting(*pref_service());

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA) || \
    BUILDFLAG(CHROME_FOR_TESTING)
  EXPECT_EQ(search_engine_choice_service().GetStaticChoiceScreenConditions(
                policy_service(), /*is_regular_profile=*/true,
                template_url_service()),
            SearchEngineChoiceScreenConditions::kUnsupportedBrowserType);
#else
  EXPECT_EQ(search_engine_choice_service().GetStaticChoiceScreenConditions(
                policy_service(), /*is_regular_profile=*/true,
                template_url_service()),
            SearchEngineChoiceScreenConditions::kEligible);
  EXPECT_EQ(search_engine_choice_service().GetDynamicChoiceScreenConditions(
                template_url_service()),
            SearchEngineChoiceScreenConditions::kEligible);
#endif
}
TEST_F(SearchEngineChoiceServiceTest,
       ShowChoiceScreenWithForceCommandLineFlag_Counterfactual) {
  search_engines::MarkSearchEngineChoiceCompletedForTesting(*pref_service());

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA) || \
    BUILDFLAG(CHROME_FOR_TESTING)
  EXPECT_EQ(search_engine_choice_service().GetStaticChoiceScreenConditions(
                policy_service(), /*is_regular_profile=*/true,
                template_url_service()),
            SearchEngineChoiceScreenConditions::kUnsupportedBrowserType);
#else
  EXPECT_EQ(search_engine_choice_service().GetStaticChoiceScreenConditions(
                policy_service(), /*is_regular_profile=*/true,
                template_url_service()),
            SearchEngineChoiceScreenConditions::kAlreadyCompleted);
  EXPECT_EQ(search_engine_choice_service().GetDynamicChoiceScreenConditions(
                template_url_service()),
            SearchEngineChoiceScreenConditions::kAlreadyCompleted);
#endif
}

// Test that the choice screen does not get displayed if the command line
// argument for disabling it is set.
TEST_F(SearchEngineChoiceServiceTest,
       DoNotShowChoiceScreenWithDisableCommandLineFlag) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableSearchEngineChoiceScreen);
  EXPECT_EQ(search_engine_choice_service().GetStaticChoiceScreenConditions(
                policy_service(),
                /*is_regular_profile=*/true, template_url_service()),
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA) || \
    BUILDFLAG(CHROME_FOR_TESTING)
            SearchEngineChoiceScreenConditions::kUnsupportedBrowserType
#else

            SearchEngineChoiceScreenConditions::kFeatureSuppressed
#endif
  );
}

TEST_F(SearchEngineChoiceServiceTest, GetCountryIdCommandLineOverride) {
  // The test is set up to use the command line to simulate the country as being
  // Belgium.
  EXPECT_EQ(search_engine_choice_service().GetCountryId(), kBelgiumCountryId);

  // When removing the command line flag, the default value is based on the
  // device locale.
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      switches::kSearchEngineChoiceCountry);

  // Note that if the format matches (2-character strings), we might get a
  // country ID that is not valid/supported.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kSearchEngineChoiceCountry, "??");
  EXPECT_EQ(search_engine_choice_service().GetCountryId(),
            country_codes::CountryCharsToCountryID('?', '?'));
}

TEST_F(SearchEngineChoiceServiceTest,
       GetCountryIdCommandLineOverrideSetsToUnknownOnFormatMismatch) {
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      switches::kSearchEngineChoiceCountry);

  // When the command line value is invalid, the country code should be unknown.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kSearchEngineChoiceCountry, "USA");
  EXPECT_EQ(search_engine_choice_service().GetCountryId(),
            country_codes::kCountryIDUnknown);
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(SearchEngineChoiceServiceTest, PlayResponseBeforeGetCountryId) {
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      switches::kSearchEngineChoiceCountry);
  TestSupportAndroid test_support;
  test_support.ReturnDeviceCountry(
      country_codes::CountryIDToCountryString(kBelgiumCountryId));

  // We got response from Play API before `GetCountryId` was invoked for the
  // first time this run, so the new value should be used right away.
  EXPECT_EQ(search_engine_choice_service().GetCountryId(), kBelgiumCountryId);
  // The pref should be updated as well.
  EXPECT_EQ(pref_service()->GetInteger(country_codes::kCountryIDAtInstall),
            kBelgiumCountryId);
}

TEST_F(SearchEngineChoiceServiceTest, GetCountryIdBeforePlayResponse) {
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      switches::kSearchEngineChoiceCountry);

  TestSupportAndroid test_support;
  // We didn't get a response from Play API before `GetCountryId` was invoked,
  // so the last known country from prefs should be used.
  EXPECT_EQ(search_engine_choice_service().GetCountryId(),
            country_codes::GetCurrentCountryID());

  // Simulate a response arriving after the first `GetCountryId` call.
  test_support.ReturnDeviceCountry(
      country_codes::CountryIDToCountryString(kBelgiumCountryId));

  // The pref should be updated so the new country can be used the next run.
  EXPECT_EQ(pref_service()->GetInteger(country_codes::kCountryIDAtInstall),
            kBelgiumCountryId);
  // However, `GetCountryId` result shouldn't change until the next run.
  EXPECT_EQ(search_engine_choice_service().GetCountryId(),
            country_codes::GetCurrentCountryID());
}

TEST_F(SearchEngineChoiceServiceTest, GetCountryIdPrefAlreadyWritten) {
  // The value set from the pref should be used.
  pref_service()->SetInteger(country_codes::kCountryIDAtInstall,
                             kBelgiumCountryId);
  // Don't create `TestSupportAndroid` - since the pref isn't set we should not
  // reach out to Java.
  EXPECT_EQ(search_engine_choice_service().GetCountryId(), kBelgiumCountryId);
}
#endif  // BUILDFLAG(IS_ANDROID)

// On Android, internal device APIs are used to get the current country.
#if !BUILDFLAG(IS_ANDROID)
TEST_F(SearchEngineChoiceServiceTest, GetCountryIdDefault) {
  // Remove the command line flag set by the test.
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      switches::kSearchEngineChoiceCountry);

  // The default value should be based on the device locale.
  EXPECT_EQ(search_engine_choice_service().GetCountryId(),
            country_codes::GetCurrentCountryID());
}

TEST_F(SearchEngineChoiceServiceTest, GetCountryIdFromPrefs) {
  // Remove the command line flag set by the test.
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      switches::kSearchEngineChoiceCountry);

  // The value set from the pref should be used.
  pref_service()->SetInteger(country_codes::kCountryIDAtInstall,
                             kBelgiumCountryId);
  EXPECT_EQ(search_engine_choice_service().GetCountryId(), kBelgiumCountryId);
}

TEST_F(SearchEngineChoiceServiceTest, GetCountryIdChangesAfterReading) {
  // Remove the command line flag set by the test.
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      switches::kSearchEngineChoiceCountry);

  // The value set from the pref should be used.
  pref_service()->SetInteger(country_codes::kCountryIDAtInstall,
                             kBelgiumCountryId);
  EXPECT_EQ(search_engine_choice_service().GetCountryId(), kBelgiumCountryId);

  // Change the value in pref.
  pref_service()->SetInteger(country_codes::kCountryIDAtInstall,
                             country_codes::CountryCharsToCountryID('U', 'S'));
  // The value returned by `GetCountryId` shouldn't change.
  EXPECT_EQ(search_engine_choice_service().GetCountryId(), kBelgiumCountryId);
}
#endif  // !BUILDFLAG(IS_ANDROID)

TEST_F(SearchEngineChoiceServiceTest, ChoiceScreenConditions_SkipFor3p) {
  // First, check the state with Google as the default search engine
  ASSERT_TRUE(
      template_url_service().GetDefaultSearchProvider()->prepopulate_id() ==
      TemplateURLPrepopulateData::google.id);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA) || \
    BUILDFLAG(CHROME_FOR_TESTING)
  EXPECT_EQ(search_engine_choice_service().GetStaticChoiceScreenConditions(
                policy_service(), /*is_regular_profile=*/true,
                template_url_service()),
            SearchEngineChoiceScreenConditions::kUnsupportedBrowserType);
#else
  EXPECT_EQ(search_engine_choice_service().GetStaticChoiceScreenConditions(
                policy_service(), /*is_regular_profile=*/true,
                template_url_service()),
            SearchEngineChoiceScreenConditions::kEligible);
  EXPECT_EQ(search_engine_choice_service().GetDynamicChoiceScreenConditions(
                template_url_service()),
            SearchEngineChoiceScreenConditions::kEligible);
#endif

  // Second, check the state after changing the default search engine.
  std::unique_ptr<TemplateURLData> template_url_data =
      TemplateURLDataFromPrepopulatedEngine(TemplateURLPrepopulateData::bing);
  template_url_service().SetUserSelectedDefaultSearchProvider(
      template_url_service().Add(
          std::make_unique<TemplateURL>(*template_url_data.get())));

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA) || \
    BUILDFLAG(CHROME_FOR_TESTING)
  EXPECT_EQ(search_engine_choice_service().GetStaticChoiceScreenConditions(
                policy_service(), /*is_regular_profile=*/true,
                template_url_service()),
            SearchEngineChoiceScreenConditions::kUnsupportedBrowserType);
#else
  EXPECT_EQ(search_engine_choice_service().GetStaticChoiceScreenConditions(
                policy_service(), /*is_regular_profile=*/true,
                template_url_service()),
            SearchEngineChoiceScreenConditions::kEligible);
  EXPECT_EQ(search_engine_choice_service().GetDynamicChoiceScreenConditions(
                template_url_service()),
            SearchEngineChoiceScreenConditions::kHasNonGoogleSearchEngine);
#endif
}

TEST_F(SearchEngineChoiceServiceTest,
       DoNotShowChoiceScreenIfUserHasCustomSearchEngineSetAsDefault) {
  // A custom search engine will have a `prepopulate_id` of 0.
  const int kCustomSearchEnginePrepopulateId = 0;
  TemplateURLData template_url_data;
  template_url_data.prepopulate_id = kCustomSearchEnginePrepopulateId;
  template_url_data.SetURL("https://www.example.com/?q={searchTerms}");
  template_url_service().SetUserSelectedDefaultSearchProvider(
      template_url_service().Add(
          std::make_unique<TemplateURL>(template_url_data)));

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA) || \
    BUILDFLAG(CHROME_FOR_TESTING)
  EXPECT_EQ(search_engine_choice_service().GetStaticChoiceScreenConditions(
                policy_service(), /*is_regular_profile=*/true,
                template_url_service()),
            SearchEngineChoiceScreenConditions::kUnsupportedBrowserType);
#else
  EXPECT_EQ(search_engine_choice_service().GetStaticChoiceScreenConditions(
                policy_service(), /*is_regular_profile=*/true,
                template_url_service()),
            SearchEngineChoiceScreenConditions::kEligible);
  EXPECT_EQ(search_engine_choice_service().GetDynamicChoiceScreenConditions(
                template_url_service()),
            SearchEngineChoiceScreenConditions::kHasNonGoogleSearchEngine);
#endif
}

TEST_F(SearchEngineChoiceServiceTest, RecordChoiceMade) {
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      switches::kSearchEngineChoiceCountry);
  // Test that the choice is not recorded for countries outside the EEA region.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kSearchEngineChoiceCountry, "US");

  const TemplateURL* default_search_engine =
      template_url_service().GetDefaultSearchProvider();
  EXPECT_EQ(default_search_engine->prepopulate_id(),
            TemplateURLPrepopulateData::google.id);

  search_engine_choice_service().RecordChoiceMade(
      search_engines::ChoiceMadeLocation::kChoiceScreen,
      &template_url_service());

  histogram_tester_.ExpectUniqueSample(
      search_engines::kSearchEngineChoiceScreenDefaultSearchEngineTypeHistogram,
      SearchEngineType::SEARCH_ENGINE_GOOGLE, 0);
  EXPECT_FALSE(pref_service()->HasPrefPath(
      prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp));
  EXPECT_FALSE(pref_service()->HasPrefPath(
      prefs::kDefaultSearchProviderChoiceScreenCompletionVersion));

  // Revert to an EEA region country.
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      switches::kSearchEngineChoiceCountry);
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kSearchEngineChoiceCountry,
      country_codes::CountryIDToCountryString(kBelgiumCountryId));

  // Test that the choice is recorded if it wasn't previously done.
  search_engine_choice_service().RecordChoiceMade(
      search_engines::ChoiceMadeLocation::kChoiceScreen,
      &template_url_service());
  histogram_tester_.ExpectUniqueSample(
      search_engines::kSearchEngineChoiceScreenDefaultSearchEngineTypeHistogram,
      SearchEngineType::SEARCH_ENGINE_GOOGLE, 1);

  EXPECT_NEAR(pref_service()->GetInt64(
                  prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp),
              base::Time::Now().ToDeltaSinceWindowsEpoch().InSeconds(),
              /*abs_error=*/2);
  EXPECT_EQ(pref_service()->GetString(
                prefs::kDefaultSearchProviderChoiceScreenCompletionVersion),
            version_info::GetVersionNumber());

  // Set the pref to 5 so that we can know if it gets modified.
  const int kModifiedTimestamp = 5;
  pref_service()->SetInt64(
      prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp,
      kModifiedTimestamp);

  // Test that the choice is not recorded if it was previously done.
  search_engine_choice_service().RecordChoiceMade(
      search_engines::ChoiceMadeLocation::kChoiceScreen,
      &template_url_service());
  EXPECT_EQ(pref_service()->GetInt64(
                prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp),
            kModifiedTimestamp);

  histogram_tester_.ExpectUniqueSample(
      search_engines::kSearchEngineChoiceScreenDefaultSearchEngineTypeHistogram,
      SearchEngineType::SEARCH_ENGINE_GOOGLE, 1);
}

TEST_F(SearchEngineChoiceServiceTest, RecordChoiceMade_DistributionCustom) {
  // A distribution custom search engine will have a `prepopulate_id` > 1000.
  const int kDistributionCustomSearchEnginePrepopulateId = 1001;
  TemplateURLData template_url_data;
  template_url_data.prepopulate_id =
      kDistributionCustomSearchEnginePrepopulateId;
  template_url_data.SetURL("https://www.example.com/?q={searchTerms}");
  template_url_service().SetUserSelectedDefaultSearchProvider(
      template_url_service().Add(
          std::make_unique<TemplateURL>(template_url_data)));

  // Revert to an EEA region country.
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      switches::kSearchEngineChoiceCountry);
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kSearchEngineChoiceCountry,
      country_codes::CountryIDToCountryString(kBelgiumCountryId));

  // Test that the choice is recorded if it wasn't previously done.
  search_engine_choice_service().RecordChoiceMade(
      search_engines::ChoiceMadeLocation::kChoiceScreen,
      &template_url_service());
  histogram_tester_.ExpectBucketCount(
      search_engines::kSearchEngineChoiceScreenDefaultSearchEngineTypeHistogram,
      SearchEngineType::SEARCH_ENGINE_OTHER, 1);

  EXPECT_NEAR(pref_service()->GetInt64(
                  prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp),
              base::Time::Now().ToDeltaSinceWindowsEpoch().InSeconds(),
              /*abs_error=*/2);
  EXPECT_EQ(pref_service()->GetString(
                prefs::kDefaultSearchProviderChoiceScreenCompletionVersion),
            version_info::GetVersionNumber());
}

TEST_F(SearchEngineChoiceServiceTest, RecordChoiceMade_RemovedPrepopulated) {
  // We don't have a prepopulated search engine with ID 20, but at some point
  // in the past, we did. So some profiles might still have it.
  const int kRemovedSearchEnginePrepopulateId = 20;
  TemplateURLData template_url_data;
  template_url_data.prepopulate_id = kRemovedSearchEnginePrepopulateId;
  template_url_data.SetURL("https://www.example.com/?q={searchTerms}");
  template_url_service().SetUserSelectedDefaultSearchProvider(
      template_url_service().Add(
          std::make_unique<TemplateURL>(template_url_data)));

  // Revert to an EEA region country.
  base::CommandLine::ForCurrentProcess()->RemoveSwitch(
      switches::kSearchEngineChoiceCountry);
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kSearchEngineChoiceCountry,
      country_codes::CountryIDToCountryString(kBelgiumCountryId));

  // Test that the choice is recorded if it wasn't previously done.
  search_engine_choice_service().RecordChoiceMade(
      search_engines::ChoiceMadeLocation::kChoiceScreen,
      &template_url_service());
  histogram_tester_.ExpectBucketCount(
      search_engines::kSearchEngineChoiceScreenDefaultSearchEngineTypeHistogram,
      SearchEngineType::SEARCH_ENGINE_OTHER, 1);

  EXPECT_NEAR(pref_service()->GetInt64(
                  prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp),
              base::Time::Now().ToDeltaSinceWindowsEpoch().InSeconds(),
              /*abs_error=*/2);
  EXPECT_EQ(pref_service()->GetString(
                prefs::kDefaultSearchProviderChoiceScreenCompletionVersion),
            version_info::GetVersionNumber());
}

TemplateURL::OwnedTemplateURLVector
OwnedTemplateURLVectorFromPrepopulatedEngines(
    const std::vector<const TemplateURLPrepopulateData::PrepopulatedEngine*>&
        engines) {
  TemplateURL::OwnedTemplateURLVector result;
  for (const TemplateURLPrepopulateData::PrepopulatedEngine* engine : engines) {
    result.push_back(std::make_unique<TemplateURL>(
        *TemplateURLDataFromPrepopulatedEngine(*engine)));
  }
  return result;
}

TEST_F(SearchEngineChoiceServiceTest, MaybeRecordChoiceScreenDisplayState) {
  InitService(kBelgiumCountryId, true);
  ChoiceScreenData choice_screen_data(
      OwnedTemplateURLVectorFromPrepopulatedEngines(
          {&TemplateURLPrepopulateData::google,
           &TemplateURLPrepopulateData::bing,
           &TemplateURLPrepopulateData::yahoo}),
      kBelgiumCountryId, SearchTermsData());
  ChoiceScreenDisplayState display_state = choice_screen_data.display_state();
  display_state.selected_engine_index = 2;

  base::HistogramTester histogram_tester;
  search_engine_choice_service().MaybeRecordChoiceScreenDisplayState(
      display_state);

  histogram_tester.ExpectUniqueSample(
      kSearchEngineChoiceScreenSelectedEngineIndexHistogram, 2, 1);
  histogram_tester.ExpectBucketCount(
      kSearchEngineChoiceScreenShowedEngineAtCountryMismatchHistogram, false,
      1);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(
          kSearchEngineChoiceScreenShowedEngineAtHistogramPattern, 0),
      SEARCH_ENGINE_GOOGLE, 1);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(
          kSearchEngineChoiceScreenShowedEngineAtHistogramPattern, 1),
      SEARCH_ENGINE_BING, 1);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(
          kSearchEngineChoiceScreenShowedEngineAtHistogramPattern, 2),
      SEARCH_ENGINE_YAHOO, 1);

  // There is no search engine shown at index 3, since we have only 3 options.
  histogram_tester.ExpectTotalCount(
      base::StringPrintf(
          kSearchEngineChoiceScreenShowedEngineAtHistogramPattern, 3),
      0);

  // We logged the display state, so we don't need to cache it.
  EXPECT_FALSE(pref_service()->HasPrefPath(
      prefs::kDefaultSearchProviderPendingChoiceScreenDisplayState));
}

TEST_F(SearchEngineChoiceServiceTest,
       MaybeRecordChoiceScreenDisplayState_NoopUnsupportedCountry) {
  auto engines = {&TemplateURLPrepopulateData::google,
                  &TemplateURLPrepopulateData::bing,
                  &TemplateURLPrepopulateData::yahoo};
  base::HistogramTester histogram_tester;

  {
    // Unknown country.
    InitService(country_codes::kCountryIDUnknown, /*force_reset=*/true);
    ChoiceScreenData choice_screen_data(
        OwnedTemplateURLVectorFromPrepopulatedEngines(engines),
        country_codes::kCountryIDUnknown, SearchTermsData());
    ChoiceScreenDisplayState display_state = choice_screen_data.display_state();
    display_state.selected_engine_index = 0;

    search_engine_choice_service().MaybeRecordChoiceScreenDisplayState(
        display_state);
  }

  histogram_tester.ExpectTotalCount(
      kSearchEngineChoiceScreenSelectedEngineIndexHistogram, 0);
  histogram_tester.ExpectTotalCount(
      kSearchEngineChoiceScreenShowedEngineAtCountryMismatchHistogram, 0);

  // The choice is coming from a non-eea country and won't be logged, don't
  // cache it.
  EXPECT_FALSE(pref_service()->HasPrefPath(
      prefs::kDefaultSearchProviderPendingChoiceScreenDisplayState));

  {
    // Non-EEA country.
    const int kUsaCountryId = country_codes::CountryStringToCountryID("US");
    InitService(kUsaCountryId, /*force_reset=*/true);
    ChoiceScreenData choice_screen_data(
        OwnedTemplateURLVectorFromPrepopulatedEngines(engines), kUsaCountryId,
        SearchTermsData());
    ChoiceScreenDisplayState display_state = choice_screen_data.display_state();
    display_state.selected_engine_index = 0;
    search_engine_choice_service().MaybeRecordChoiceScreenDisplayState(
        display_state);
  }

  histogram_tester.ExpectTotalCount(
      kSearchEngineChoiceScreenSelectedEngineIndexHistogram, 0);
  histogram_tester.ExpectTotalCount(
      kSearchEngineChoiceScreenShowedEngineAtCountryMismatchHistogram, 0);

  // The choice is coming from a non-eea country and won't be logged, don't
  // cache it.
  EXPECT_FALSE(pref_service()->HasPrefPath(
      prefs::kDefaultSearchProviderPendingChoiceScreenDisplayState));
}

TEST_F(SearchEngineChoiceServiceTest,
       MaybeRecordChoiceScreenDisplayState_MismatchingCountry) {
  auto engines = {&TemplateURLPrepopulateData::google,
                  &TemplateURLPrepopulateData::bing,
                  &TemplateURLPrepopulateData::yahoo};
  base::HistogramTester histogram_tester;

  // Mismatch between the variations and choice screen data country.
  InitService(country_codes::CountryStringToCountryID("DE"),
              /*force_reset=*/true);
  ChoiceScreenData choice_screen_data(
      OwnedTemplateURLVectorFromPrepopulatedEngines(engines), kBelgiumCountryId,
      SearchTermsData());
  ChoiceScreenDisplayState display_state = choice_screen_data.display_state();
  display_state.selected_engine_index = 0;
  search_engine_choice_service().MaybeRecordChoiceScreenDisplayState(
      display_state);

  histogram_tester.ExpectBucketCount(
      kSearchEngineChoiceScreenSelectedEngineIndexHistogram, 0, 1);
  histogram_tester.ExpectBucketCount(
      kSearchEngineChoiceScreenShowedEngineAtCountryMismatchHistogram, true, 1);

  // None of the above should have logged the full list of indices.
  histogram_tester.ExpectTotalCount(
      base::StringPrintf(
          kSearchEngineChoiceScreenShowedEngineAtHistogramPattern, 0),
      0);
  histogram_tester.ExpectTotalCount(
      base::StringPrintf(
          kSearchEngineChoiceScreenShowedEngineAtHistogramPattern, 1),
      0);
  histogram_tester.ExpectTotalCount(
      base::StringPrintf(
          kSearchEngineChoiceScreenShowedEngineAtHistogramPattern, 2),
      0);
  histogram_tester.ExpectTotalCount(
      base::StringPrintf(
          kSearchEngineChoiceScreenShowedEngineAtHistogramPattern, 3),
      0);

  // The choice screen state should be cached for a next chance later.
  EXPECT_TRUE(pref_service()->HasPrefPath(
      prefs::kDefaultSearchProviderPendingChoiceScreenDisplayState));

  auto stored_display_state =
      ChoiceScreenDisplayState::FromDict(pref_service()->GetDict(
          prefs::kDefaultSearchProviderPendingChoiceScreenDisplayState));
  EXPECT_EQ(stored_display_state->search_engines, display_state.search_engines);
  EXPECT_EQ(stored_display_state->country_id, display_state.country_id);
  EXPECT_EQ(stored_display_state->selected_engine_index,
            display_state.selected_engine_index);
}

TEST_F(SearchEngineChoiceServiceTest,
       MaybeRecordChoiceScreenDisplayState_OnServiceStartup) {
  ChoiceScreenDisplayState display_state(
      /*search_engines=*/{SEARCH_ENGINE_GOOGLE, SEARCH_ENGINE_BING,
                          SEARCH_ENGINE_YAHOO},
      /*country_id=*/kBelgiumCountryId,
      /*selected_engine_index=*/0);
  pref_service()->SetDict(
      prefs::kDefaultSearchProviderPendingChoiceScreenDisplayState,
      display_state.ToDict());
  search_engines::MarkSearchEngineChoiceCompletedForTesting(*pref_service());

  base::HistogramTester histogram_tester;
  InitService(kBelgiumCountryId, /*force_reset=*/true);

  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(
          kSearchEngineChoiceScreenShowedEngineAtHistogramPattern, 0),
      SEARCH_ENGINE_GOOGLE, 1);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(
          kSearchEngineChoiceScreenShowedEngineAtHistogramPattern, 1),
      SEARCH_ENGINE_BING, 1);
  histogram_tester.ExpectUniqueSample(
      base::StringPrintf(
          kSearchEngineChoiceScreenShowedEngineAtHistogramPattern, 2),
      SEARCH_ENGINE_YAHOO, 1);

  // These metrics are expected to have been already logged at the time we
  // cached the screen state.
  histogram_tester.ExpectTotalCount(
      kSearchEngineChoiceScreenSelectedEngineIndexHistogram, 0);
  histogram_tester.ExpectTotalCount(
      kSearchEngineChoiceScreenShowedEngineAtCountryMismatchHistogram, 0);

  // The choice screen state should now be cleared.
  EXPECT_FALSE(pref_service()->HasPrefPath(
      prefs::kDefaultSearchProviderPendingChoiceScreenDisplayState));
}

TEST_F(SearchEngineChoiceServiceTest,
       MaybeRecordChoiceScreenDisplayState_OnServiceStartup_CountryMismatch) {
  ChoiceScreenDisplayState display_state(
      /*search_engines=*/{SEARCH_ENGINE_GOOGLE, SEARCH_ENGINE_BING,
                          SEARCH_ENGINE_YAHOO},
      /*country_id=*/kBelgiumCountryId,
      /*selected_engine_index=*/0);
  pref_service()->SetDict(
      prefs::kDefaultSearchProviderPendingChoiceScreenDisplayState,
      display_state.ToDict());
  search_engines::MarkSearchEngineChoiceCompletedForTesting(*pref_service());

  base::HistogramTester histogram_tester;
  InitService(country_codes::kCountryIDUnknown, /*force_reset=*/true);

  histogram_tester.ExpectTotalCount(
      base::StringPrintf(
          kSearchEngineChoiceScreenShowedEngineAtHistogramPattern, 0),
      0);
  histogram_tester.ExpectTotalCount(
      base::StringPrintf(
          kSearchEngineChoiceScreenShowedEngineAtHistogramPattern, 1),
      0);
  histogram_tester.ExpectTotalCount(
      base::StringPrintf(
          kSearchEngineChoiceScreenShowedEngineAtHistogramPattern, 2),
      0);

  // These metrics are expected to have been already logged at the time we
  // cached the screen state.
  histogram_tester.ExpectTotalCount(
      kSearchEngineChoiceScreenSelectedEngineIndexHistogram, 0);
  histogram_tester.ExpectTotalCount(
      kSearchEngineChoiceScreenShowedEngineAtCountryMismatchHistogram, 0);

  // The choice screen state should stay around.
  EXPECT_TRUE(pref_service()->HasPrefPath(
      prefs::kDefaultSearchProviderPendingChoiceScreenDisplayState));
}

TEST_F(SearchEngineChoiceServiceTest,
       MaybeRecordChoiceScreenDisplayState_OnServiceStartup_ChoicePrefCleared) {
  ChoiceScreenDisplayState display_state(
      /*search_engines=*/{SEARCH_ENGINE_GOOGLE, SEARCH_ENGINE_BING,
                          SEARCH_ENGINE_YAHOO},
      /*country_id=*/kBelgiumCountryId,
      /*selected_engine_index=*/0);
  pref_service()->SetDict(
      prefs::kDefaultSearchProviderPendingChoiceScreenDisplayState,
      display_state.ToDict());

  base::HistogramTester histogram_tester;
  InitService(country_codes::kCountryIDUnknown, /*force_reset=*/true);

  histogram_tester.ExpectTotalCount(
      kSearchEngineChoiceScreenShowedEngineAtCountryMismatchHistogram, 0);
  histogram_tester.ExpectTotalCount(
      base::StringPrintf(
          kSearchEngineChoiceScreenShowedEngineAtHistogramPattern, 0),
      0);
  histogram_tester.ExpectTotalCount(
      base::StringPrintf(
          kSearchEngineChoiceScreenShowedEngineAtHistogramPattern, 1),
      0);
  histogram_tester.ExpectTotalCount(
      base::StringPrintf(
          kSearchEngineChoiceScreenShowedEngineAtHistogramPattern, 2),
      0);

  // Choice not marked done, so the service also clear the pending state.
  EXPECT_FALSE(pref_service()->HasPrefPath(
      prefs::kDefaultSearchProviderPendingChoiceScreenDisplayState));
}

TEST_F(SearchEngineChoiceServiceTest,
       MaybeRecordChoiceScreenDisplayState_OnServiceStartup_UmaDisabled) {
  // Disable UMA reporting.
  SearchEngineChoiceMetricsServiceAccessor::
      SetForceIsMetricsReportingEnabledPrefLookup(false);

  ChoiceScreenDisplayState display_state(
      /*search_engines=*/{SEARCH_ENGINE_GOOGLE, SEARCH_ENGINE_BING,
                          SEARCH_ENGINE_YAHOO},
      /*country_id=*/kBelgiumCountryId,
      /*selected_engine_index=*/0);
  pref_service()->SetDict(
      prefs::kDefaultSearchProviderPendingChoiceScreenDisplayState,
      display_state.ToDict());
  EXPECT_TRUE(pref_service()->HasPrefPath(
      prefs::kDefaultSearchProviderPendingChoiceScreenDisplayState));

  InitService(kBelgiumCountryId, /*force_reset=*/true);
  EXPECT_FALSE(pref_service()->HasPrefPath(
      prefs::kDefaultSearchProviderPendingChoiceScreenDisplayState));

  histogram_tester_.ExpectTotalCount(
      base::StringPrintf(
          kSearchEngineChoiceScreenShowedEngineAtHistogramPattern, 0),
      0);
  histogram_tester_.ExpectTotalCount(
      kSearchEngineChoiceScreenShowedEngineAtCountryMismatchHistogram, 0);
}

// Test that the user is not reprompted if the reprompt parameter is not a valid
// JSON string.
TEST_F(SearchEngineChoiceServiceTest, NoRepromptForSyntaxError) {
  // Set the reprompt parameters with invalid syntax.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      switches::kSearchEngineChoiceTrigger,
      {{switches::kSearchEngineChoiceTriggerRepromptParams.name, "Foo"}});
  ASSERT_EQ("Foo", switches::kSearchEngineChoiceTriggerRepromptParams.Get());

  // Initialize the preference with some previous choice.
  int64_t kPreviousTimestamp = 1;
  pref_service()->SetInt64(
      prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp,
      kPreviousTimestamp);

  // Trigger the creation of the service, which should check for the reprompt.
  search_engine_choice_service();

  // The user should not be reprompted.
  EXPECT_EQ(kPreviousTimestamp,
            pref_service()->GetInt64(
                prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp));
  histogram_tester_.ExpectTotalCount(
      search_engines::kSearchEngineChoiceWipeReasonHistogram, 0);
  histogram_tester_.ExpectTotalCount(
      search_engines::kSearchEngineChoiceRepromptSpecificCountryHistogram, 0);
  histogram_tester_.ExpectTotalCount(
      search_engines::kSearchEngineChoiceRepromptWildcardHistogram, 0);
  histogram_tester_.ExpectUniqueSample(
      search_engines::kSearchEngineChoiceRepromptHistogram,
      RepromptResult::kInvalidDictionary, 1);
}

// Test that the user is not reprompted by default.
TEST_F(SearchEngineChoiceServiceTest, NoRepromptByDefault) {
  ASSERT_EQ(switches::kSearchEngineChoiceNoRepromptString,
            switches::kSearchEngineChoiceTriggerRepromptParams.Get());

  // Initialize the preference with some previous choice.
  int64_t kPreviousTimestamp = 1;
  pref_service()->SetInt64(
      prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp,
      kPreviousTimestamp);

  // Trigger the creation of the service, which should check for the reprompt.
  search_engine_choice_service();

  // The user should not be reprompted.
  EXPECT_EQ(kPreviousTimestamp,
            pref_service()->GetInt64(
                prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp));
  histogram_tester_.ExpectTotalCount(
      search_engines::kSearchEngineChoiceWipeReasonHistogram, 0);
  histogram_tester_.ExpectTotalCount(
      search_engines::kSearchEngineChoiceRepromptSpecificCountryHistogram, 0);
  histogram_tester_.ExpectTotalCount(
      search_engines::kSearchEngineChoiceRepromptWildcardHistogram, 0);
  histogram_tester_.ExpectUniqueSample(
      search_engines::kSearchEngineChoiceRepromptHistogram,
      RepromptResult::kNoReprompt, 1);
  histogram_tester_.ExpectBucketCount(
      search_engines::kSearchEngineChoiceRepromptHistogram,
      RepromptResult::kInvalidDictionary, 0);
}

// The user is reprompted if the version preference is missing.
TEST_F(SearchEngineChoiceServiceTest, RepromptForMissingChoiceVersion) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      switches::kSearchEngineChoiceTrigger,
      {{switches::kSearchEngineChoiceTriggerRepromptParams.name, "{}"}});
  ASSERT_EQ("{}", switches::kSearchEngineChoiceTriggerRepromptParams.Get());

  // Initialize the timestamp, but not the version.
  int64_t kPreviousTimestamp = 1;
  pref_service()->SetInt64(
      prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp,
      kPreviousTimestamp);

  // Trigger the creation of the service, which should check for the reprompt.
  search_engine_choice_service();

  // The user should be reprompted.
  EXPECT_FALSE(pref_service()->HasPrefPath(
      prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp));
  EXPECT_FALSE(pref_service()->HasPrefPath(
      prefs::kDefaultSearchProviderChoiceScreenCompletionVersion));
  histogram_tester_.ExpectUniqueSample(
      search_engines::kSearchEngineChoiceWipeReasonHistogram,
      WipeSearchEngineChoiceReason::kMissingChoiceVersion, 1);
  histogram_tester_.ExpectTotalCount(
      search_engines::kSearchEngineChoiceRepromptSpecificCountryHistogram, 0);
  histogram_tester_.ExpectTotalCount(
      search_engines::kSearchEngineChoiceRepromptWildcardHistogram, 0);
  histogram_tester_.ExpectTotalCount(
      search_engines::kSearchEngineChoiceRepromptHistogram, 0);
}

struct RepromptTestParam {
  // Whether the user should be reprompted or not.
  std::optional<WipeSearchEngineChoiceReason> wipe_reason;
  // Internal results of the reprompt computation.
  std::optional<RepromptResult> wildcard_result;
  std::optional<RepromptResult> country_result;
  // Version of the choice.
  const std::string_view choice_version;
  // The reprompt params, as sent by the server.  Use `CURRENT_VERSION` for the
  // current Chrome version, and `FUTURE_VERSION` for a future version.
  const char* param_dict;
};

class SearchEngineChoiceUtilsParamTest
    : public SearchEngineChoiceServiceTest,
      public testing::WithParamInterface<RepromptTestParam> {};

TEST_P(SearchEngineChoiceUtilsParamTest, Reprompt) {
  // Set the reprompt parameters.
  std::string param_dict = base::CollapseWhitespaceASCII(
      GetParam().param_dict, /*trim_sequences_with_line_breaks=*/true);
  base::ReplaceFirstSubstringAfterOffset(&param_dict, /*start_offset=*/0,
                                         "CURRENT_VERSION",
                                         version_info::GetVersionNumber());
  base::Version future_version(
      {version_info::GetMajorVersionNumberAsInt() + 5u, 2, 3, 4});
  ASSERT_TRUE(future_version.IsValid());
  base::ReplaceFirstSubstringAfterOffset(&param_dict, /*start_offset=*/0,
                                         "FUTURE_VERSION",
                                         future_version.GetString());

  std::string feature_param_value = param_dict.empty() ? "{}" : param_dict;

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      switches::kSearchEngineChoiceTrigger,
      {{switches::kSearchEngineChoiceTriggerRepromptParams.name,
        feature_param_value}});
  ASSERT_EQ(feature_param_value,
            switches::kSearchEngineChoiceTriggerRepromptParams.Get());

  // Initialize the preference with some previous choice.
  int64_t kPreviousTimestamp = 1;
  pref_service()->SetInt64(
      prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp,
      kPreviousTimestamp);
  pref_service()->SetString(
      prefs::kDefaultSearchProviderChoiceScreenCompletionVersion,
      GetParam().choice_version);

  // Trigger the creation of the service, which should check for the reprompt.
  search_engine_choice_service();

  if (GetParam().wipe_reason) {
    // User is reprompted, prefs were wiped.
    EXPECT_FALSE(pref_service()->HasPrefPath(
        prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp));
    EXPECT_FALSE(pref_service()->HasPrefPath(
        prefs::kDefaultSearchProviderChoiceScreenCompletionVersion));
    histogram_tester_.ExpectUniqueSample(
        search_engines::kSearchEngineChoiceWipeReasonHistogram,
        *GetParam().wipe_reason, 1);
  } else {
    // User is not reprompted, prefs were not wiped.
    EXPECT_EQ(
        kPreviousTimestamp,
        pref_service()->GetInt64(
            prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp));
    EXPECT_EQ(GetParam().choice_version,
              pref_service()->GetString(
                  prefs::kDefaultSearchProviderChoiceScreenCompletionVersion));
    histogram_tester_.ExpectTotalCount(
        search_engines::kSearchEngineChoiceWipeReasonHistogram, 0);
  }

  int total_reprompt_record_count = 0;
  if (GetParam().wildcard_result) {
    ++total_reprompt_record_count;
    histogram_tester_.ExpectUniqueSample(
        search_engines::kSearchEngineChoiceRepromptWildcardHistogram,
        *GetParam().wildcard_result, 1);
    EXPECT_GE(histogram_tester_.GetBucketCount(
                  search_engines::kSearchEngineChoiceRepromptHistogram,
                  *GetParam().wildcard_result),
              1);
  } else {
    histogram_tester_.ExpectTotalCount(
        search_engines::kSearchEngineChoiceRepromptWildcardHistogram, 0);
  }
  if (GetParam().country_result) {
    ++total_reprompt_record_count;
    histogram_tester_.ExpectUniqueSample(
        search_engines::kSearchEngineChoiceRepromptSpecificCountryHistogram,
        *GetParam().country_result, 1);
    EXPECT_GE(histogram_tester_.GetBucketCount(
                  search_engines::kSearchEngineChoiceRepromptHistogram,
                  *GetParam().country_result),
              1);
  } else {
    histogram_tester_.ExpectTotalCount(
        search_engines::kSearchEngineChoiceRepromptSpecificCountryHistogram, 0);
  }
  histogram_tester_.ExpectTotalCount(
      search_engines::kSearchEngineChoiceRepromptHistogram,
      total_reprompt_record_count);
}

constexpr RepromptTestParam kRepromptTestParams[] = {
    // Reprompt all countries with the wildcard.
    {WipeSearchEngineChoiceReason::kReprompt, RepromptResult::kReprompt,
     RepromptResult::kNoDictionaryKey, "1.0.0.0", R"( {"*":"1.0.0.1"} )"},
    // Reprompt works with all version components.
    {WipeSearchEngineChoiceReason::kReprompt, RepromptResult::kReprompt,
     RepromptResult::kNoDictionaryKey, "1.0.0.100", R"( {"*":"1.0.1.0"} )"},
    {WipeSearchEngineChoiceReason::kReprompt, RepromptResult::kReprompt,
     RepromptResult::kNoDictionaryKey, "1.0.200.0", R"( {"*":"1.1.0.0"} )"},
    {WipeSearchEngineChoiceReason::kReprompt, RepromptResult::kReprompt,
     RepromptResult::kNoDictionaryKey, "1.300.0.0", R"( {"*":"2.0.0.0"} )"},
    {WipeSearchEngineChoiceReason::kReprompt, RepromptResult::kReprompt,
     RepromptResult::kNoDictionaryKey, "10.10.1.1",
     R"( {"*":"30.45.678.9100"} )"},
    // Reprompt a specific country.
    {WipeSearchEngineChoiceReason::kReprompt, std::nullopt,
     RepromptResult::kReprompt, "1.0.0.0", R"( {"BE":"1.0.0.1"} )"},
    // Reprompt for params inclusive of current version
    {WipeSearchEngineChoiceReason::kReprompt, std::nullopt,
     RepromptResult::kReprompt, "1.0.0.0", R"( {"BE":"CURRENT_VERSION"} )"},
    // Reprompt when the choice version is malformed.
    {WipeSearchEngineChoiceReason::kInvalidChoiceVersion, std::nullopt,
     std::nullopt, "Blah", ""},
    // Reprompt when both the country and the wild card are specified, as long
    // as one of them qualifies.
    {WipeSearchEngineChoiceReason::kReprompt, std::nullopt,
     RepromptResult::kReprompt, "1.0.0.0",
     R"( {"*":"1.0.0.1","BE":"1.0.0.1"} )"},
    {WipeSearchEngineChoiceReason::kReprompt, std::nullopt,
     RepromptResult::kReprompt, "1.0.0.0",
     R"( {"*":"FUTURE_VERSION","BE":"1.0.0.1"} )"},
    // Still works with irrelevant parameters for other countries.
    {WipeSearchEngineChoiceReason::kReprompt, std::nullopt,
     RepromptResult::kReprompt, "1.0.0.0",
     R"(
       {
         "FR":"FUTURE_VERSION",
         "INVALID_COUNTRY":"INVALID_VERSION",
         "US":"FUTURE_VERSION",
         "BE":"1.0.0.1"
       } )"},

    // Don't reprompt when the choice was made in the current version.
    {std::nullopt, RepromptResult::kRecentChoice,
     RepromptResult::kNoDictionaryKey, version_info::GetVersionNumber(),
     "{\"*\":\"CURRENT_VERSION\"}"},
    // Don't reprompt when the choice was recent enough.
    {std::nullopt, RepromptResult::kRecentChoice,
     RepromptResult::kNoDictionaryKey, "2.0.0.0", R"( {"*":"1.0.0.1"} )"},
    // Don't reprompt for another country.
    {std::nullopt, RepromptResult::kNoDictionaryKey,
     RepromptResult::kNoDictionaryKey, "1.0.0.0", R"( {"FR":"1.0.0.1"} )"},
    {std::nullopt, RepromptResult::kNoDictionaryKey,
     RepromptResult::kNoDictionaryKey, "1.0.0.0", R"( {"US":"1.0.0.1"} )"},
    {std::nullopt, RepromptResult::kNoDictionaryKey,
     RepromptResult::kNoDictionaryKey, "1.0.0.0", R"( {"XX":"1.0.0.1"} )"},
    {std::nullopt, RepromptResult::kNoDictionaryKey,
     RepromptResult::kNoDictionaryKey, "1.0.0.0",
     R"( {"INVALID_COUNTRY":"1.0.0.1"} )"},
    {std::nullopt, std::nullopt, RepromptResult::kChromeTooOld, "1.0.0.0",
     R"( {"FR":"1.0.0.1","BE":"FUTURE_VERSION"} )"},
    // Don't reprompt for future versions.
    {std::nullopt, RepromptResult::kChromeTooOld,
     RepromptResult::kNoDictionaryKey, "1.0.0.0",
     R"( {"*":"FUTURE_VERSION"} )"},
    // Wildcard is overridden by specific country.
    {std::nullopt, std::nullopt, RepromptResult::kChromeTooOld, "1.0.0.0",
     R"( {"*":"1.0.0.1","BE":"FUTURE_VERSION"} )"},
    // Combination of right version for wrong country and wrong version for
    // right country.
    {std::nullopt, std::nullopt, RepromptResult::kChromeTooOld, "2.0.0.0",
     R"(
       {
          "*":"1.1.0.0",
          "BE":"FUTURE_VERSION",
          "FR":"2.0.0.1"
        } )"},
    // Empty dictionary.
    {std::nullopt, RepromptResult::kNoDictionaryKey,
     RepromptResult::kNoDictionaryKey, "1.0.0.0", "{}"},
    // Empty parameter.
    {std::nullopt, RepromptResult::kNoDictionaryKey,
     RepromptResult::kNoDictionaryKey, "1.0.0.0", ""},
    // Wrong number of components.
    {std::nullopt, RepromptResult::kInvalidVersion,
     RepromptResult::kNoDictionaryKey, "1.0.0.0", R"( {"*":"2.0"} )"},
    // Wildcard in version.
    {std::nullopt, RepromptResult::kInvalidVersion,
     RepromptResult::kNoDictionaryKey, "1.0.0.0", R"( {"*":"2.0.0.*"} )"},
};

INSTANTIATE_TEST_SUITE_P(,
                         SearchEngineChoiceUtilsParamTest,
                         ::testing::ValuesIn(kRepromptTestParams));

#if !BUILDFLAG(IS_ANDROID)

class SearchEngineChoiceUtilsResourceIdsTest : public ::testing::Test {
 public:
  SearchEngineChoiceUtilsResourceIdsTest() {
    TemplateURLService::RegisterProfilePrefs(pref_service_.registry());
    TemplateURLPrepopulateData::RegisterProfilePrefs(pref_service_.registry());
    local_state_.registry()->RegisterBooleanPref(
        metrics::prefs::kMetricsReportingEnabled, true);

    search_engine_choice_service_ = std::make_unique<SearchEngineChoiceService>(
        pref_service_, &local_state_,
        /*is_profile_eligible_for_dse_guest_propagation=*/false);
  }

  ~SearchEngineChoiceUtilsResourceIdsTest() override = default;

  PrefService* pref_service() { return &pref_service_; }
  search_engines::SearchEngineChoiceService& search_engine_choice_service() {
    return CHECK_DEREF(search_engine_choice_service_.get());
  }

 private:
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  TestingPrefServiceSimple local_state_;
  std::unique_ptr<search_engines::SearchEngineChoiceService>
      search_engine_choice_service_;
};

// Verifies that all prepopulated search engines associated with EEA countries
// have an icon.
TEST_F(SearchEngineChoiceUtilsResourceIdsTest, GetIconResourceId) {
  // Make sure the country is not forced.
  ASSERT_FALSE(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kSearchEngineChoiceCountry));

  for (int country_id : search_engines::kEeaChoiceCountriesIds) {
    pref_service()->SetInteger(country_codes::kCountryIDAtInstall, country_id);
    std::vector<std::unique_ptr<TemplateURLData>> urls =
        TemplateURLPrepopulateData::GetPrepopulatedEngines(
            pref_service(), &search_engine_choice_service());
    for (const std::unique_ptr<TemplateURLData>& url : urls) {
      EXPECT_GE(search_engines::GetIconResourceId(url->keyword()), 0)
          << "Missing icon for " << url->keyword() << ". Try re-running "
          << "`tools/search_engine_choice/generate_search_engine_icons.py`.";
    }
  }
}

#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)

class SearchEngineChoiceServiceWithVariationsTest : public ::testing::Test {
 public:
  SearchEngineChoiceServiceWithVariationsTest() {
    TemplateURLPrepopulateData::RegisterProfilePrefs(pref_service_.registry());
    TemplateURLService::RegisterProfilePrefs(pref_service_.registry());

    local_state_.registry()->RegisterBooleanPref(
        metrics::prefs::kMetricsReportingEnabled, true);
  }

  PrefService& pref_service() { return pref_service_; }

  PrefService& local_state() { return local_state_; }

 private:
  TestingPrefServiceSimple local_state_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
};

// Tests that the country falls back to `country_codes::GetCurrentCountryID()`
// when the variations country is not available.
TEST_F(SearchEngineChoiceServiceWithVariationsTest, NoVariationsCountry) {
  ASSERT_FALSE(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kSearchEngineChoiceCountry));
  SearchEngineChoiceService search_engine_choice_service(
      pref_service(), &local_state(),
      /*is_profile_eligible_for_dse_guest_propagation=*/false,
      country_codes::kCountryIDUnknown);

  EXPECT_EQ(search_engine_choice_service.GetCountryId(),
            country_codes::GetCurrentCountryID());
}

// Tests that the country is read from the variations service when available.
TEST_F(SearchEngineChoiceServiceWithVariationsTest, WithVariationsCountry) {
  ASSERT_FALSE(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kSearchEngineChoiceCountry));

  int variation_country_id = country_codes::CountryStringToCountryID("FR");
  if (country_codes::GetCurrentCountryCode() == "FR") {
    // Make sure to use a country different from the current one.
    variation_country_id = country_codes::CountryStringToCountryID("DE");
  }

  SearchEngineChoiceService search_engine_choice_service(
      pref_service(), &local_state(),
      /*is_profile_eligible_for_dse_guest_propagation=*/false,
      variation_country_id);

  EXPECT_EQ(variation_country_id, search_engine_choice_service.GetCountryId());
}

#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)

}  // namespace search_engines
