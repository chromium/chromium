// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/version_info/version_info.h"
#include "components/country_codes/country_codes.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/policy/policy_constants.h"
#include "components/regional_capabilities/regional_capabilities_country_id.h"
#include "components/regional_capabilities/regional_capabilities_switches.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_service.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_service_test_base.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_utils.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/search_engines/search_engines_test_environment.h"
#include "components/search_engines/template_url_data_util.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_client.h"
#include "components/search_engines/util.h"
#include "components/webdata/common/web_database_service.h"
#include "components/webdata/common/webdata_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/search_engines_data/resources/definitions/prepopulated_engines.h"

namespace {

using country_codes::CountryId;
using search_engines::SearchEngineChoiceScreenConditions;
using search_engines::SearchEngineChoiceWipeReason;
using search_engines::SearchEnginesTestEnvironment;
using search_engines::WipeSearchEngineChoicePrefs;
using TemplateURLPrepopulateData::PrepopulatedEngine;

class KeywordsDatabaseHolder {
 public:
  explicit KeywordsDatabaseHolder(base::test::TaskEnvironment& task_environment)
      : task_environment_(task_environment),
        os_crypt_(os_crypt_async::GetTestOSCryptAsyncForTesting(
            /*is_sync_for_unittests=*/true)) {
    CHECK(scoped_temp_dir.CreateUniqueTempDir());
  }

  ~KeywordsDatabaseHolder() { Shutdown(); }

  void Init() {
    ASSERT_FALSE(profile_database);
    ASSERT_FALSE(keyword_web_data);

    auto task_runner = task_environment_->GetMainThreadTaskRunner();

    profile_database = base::MakeRefCounted<WebDatabaseService>(
        scoped_temp_dir.GetPath().Append(kWebDataFilename),
        /*ui_task_runner=*/task_runner,
        /*db_task_runner=*/task_runner);
    profile_database->AddTable(std::make_unique<KeywordTable>());
    profile_database->LoadDatabase(os_crypt_.get());

    keyword_web_data = base::MakeRefCounted<KeywordWebDataService>(
        profile_database, task_runner);
    keyword_web_data->Init(base::DoNothing());
  }

  void Shutdown() {
    if (keyword_web_data) {
      keyword_web_data->ShutdownOnUISequence();
      keyword_web_data = nullptr;
    }
    if (profile_database) {
      profile_database->ShutdownDatabase();
      profile_database = nullptr;
    }
  }

  raw_ref<base::test::TaskEnvironment> task_environment_;
  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt_;
  base::ScopedTempDir scoped_temp_dir;
  scoped_refptr<WebDatabaseService> profile_database;
  scoped_refptr<KeywordWebDataService> keyword_web_data;
};

SearchEngineChoiceScreenConditions IfSupported(
    SearchEngineChoiceScreenConditions condition) {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA) || \
    BUILDFLAG(CHROME_FOR_TESTING)
  return SearchEngineChoiceScreenConditions::kUnsupportedBrowserType;
#else
  return condition;
#endif
}

class SearchEngineChoiceEligibilityTest
    : public search_engines::SearchEngineChoiceServiceTestBase {
 public:
  ~SearchEngineChoiceEligibilityTest() override { ResetDeps(); }

  void ResetDeps() {
    ResetServices();  // Depends on db_holder, so reset it first.
    keywords_db_holder_.reset();
  }

  void PopulateLazyFactories(
      SearchEnginesTestEnvironment::ServiceFactories& lazy_factories,
      search_engines::InitServiceArgs args) override {
    search_engines::SearchEngineChoiceServiceTestBase::PopulateLazyFactories(
        lazy_factories, args);
    lazy_factories.template_url_service_factory = base::BindLambdaForTesting(
        [&](SearchEnginesTestEnvironment& environment) {
          keywords_db_holder_ =
              std::make_unique<KeywordsDatabaseHolder>(task_environment_);
          keywords_db_holder_->Init();

          return std::make_unique<TemplateURLService>(
              environment.pref_service(),
              environment.search_engine_choice_service(),
              environment.prepopulate_data_resolver(),
              std::make_unique<SearchTermsData>(),
              keywords_db_holder_->keyword_web_data,
              /* TemplateURLServiceClient= */ nullptr,
              /* dsp_change_callback= */ base::RepeatingClosure());
        });
  }

  void FinalizeEnvironmentInit() override {
    // Make sure TURL service loading the db is done.
    template_url_service().Load();
    task_environment_.RunUntilIdle();
  }

  SearchEngineChoiceScreenConditions GetDynamicConditions() {
    return search_engine_choice_service().GetDynamicChoiceScreenConditions(
        template_url_service());
  }

  SearchEngineChoiceScreenConditions GetStaticConditions() {
    return search_engine_choice_service().GetStaticChoiceScreenConditions(
        policy_service(), /*is_regular_profile=*/true, template_url_service());
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
  std::unique_ptr<KeywordsDatabaseHolder> keywords_db_holder_;
};

// Test that the choice screen doesn't get displayed if the profile is not
// regular.
TEST_F(SearchEngineChoiceEligibilityTest,
       DoNotShowChoiceScreenWithNotRegularProfile) {
  EXPECT_EQ(search_engine_choice_service().GetStaticChoiceScreenConditions(
                policy_service(), /*is_regular_profile=*/false,
                template_url_service()),
            SearchEngineChoiceScreenConditions::kUnsupportedBrowserType);
}

// Test that the choice screen does not get displayed if the provider list is
// overridden in the intial_preferences file.
TEST_F(SearchEngineChoiceEligibilityTest,
       DoNotShowChoiceScreenWithProviderListOverride) {
  base::Value::List override_list;
  pref_service()->SetList(prefs::kSearchProviderOverrides,
                          override_list.Clone());

  EXPECT_EQ(
      GetStaticConditions(),
      IfSupported(SearchEngineChoiceScreenConditions::kSearchProviderOverride));
}

// Test that the choice screen gets displayed if the
// `DefaultSearchProviderEnabled` policy is not set.
TEST_F(SearchEngineChoiceEligibilityTest, ShowChoiceScreenIfPoliciesAreNotSet) {
  EXPECT_EQ(GetStaticConditions(),
            IfSupported(SearchEngineChoiceScreenConditions::kEligible));
  EXPECT_EQ(GetDynamicConditions(),
            IfSupported(SearchEngineChoiceScreenConditions::kEligible));
}

// Test that the choice screen doesn't get displayed if the
// 'DefaultSearchProviderEnabled' policy is set to false.
TEST_F(SearchEngineChoiceEligibilityTest,
       DoNotShowChoiceScreenIfPolicySetToFalse) {
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

  EXPECT_EQ(
      GetStaticConditions(),
      IfSupported(SearchEngineChoiceScreenConditions::kControlledByPolicy));
  EXPECT_EQ(
      GetDynamicConditions(),
      IfSupported(SearchEngineChoiceScreenConditions::kControlledByPolicy));
}

// Test that the choice screen gets displayed if the
// 'DefaultSearchProviderEnabled' policy is set to true but the
// 'DefaultSearchProviderSearchURL' policy is not set.
TEST_F(SearchEngineChoiceEligibilityTest,
       ShowChoiceScreenIfPolicySetToTrueWithoutUrlSet) {
  policy_map().Set(policy::key::kDefaultSearchProviderEnabled,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD, base::Value(true), nullptr);

  EXPECT_EQ(GetStaticConditions(),
            IfSupported(SearchEngineChoiceScreenConditions::kEligible));
  EXPECT_EQ(GetDynamicConditions(),
            IfSupported(SearchEngineChoiceScreenConditions::kEligible));
}

// Test that the choice screen doesn't get displayed if the
// 'DefaultSearchProviderEnabled' policy is set to true and the
// DefaultSearchProviderSearchURL' is set.
TEST_F(SearchEngineChoiceEligibilityTest,
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
  dict.Set(
      DefaultSearchManager::kPolicyOrigin,
      static_cast<int>(TemplateURLData::PolicyOrigin::kDefaultSearchProvider));
  pref_service()->SetManagedPref(
      DefaultSearchManager::kDefaultSearchProviderDataPrefName,
      std::move(dict));

  ASSERT_TRUE(template_url_service().GetDefaultSearchProvider());
  ASSERT_EQ("test", template_url_service().GetDefaultSearchProvider()->url());

  EXPECT_EQ(
      GetStaticConditions(),
      IfSupported(SearchEngineChoiceScreenConditions::kControlledByPolicy));
  EXPECT_EQ(
      GetDynamicConditions(),
      IfSupported(SearchEngineChoiceScreenConditions::kControlledByPolicy));
}

// Test that the choice screen gets displayed if and only if the
// `kDefaultSearchProviderChoiceScreenTimestamp` pref is not set. Setting this
// pref means that the user has made a search engine choice in the choice
// screen.
TEST_F(SearchEngineChoiceEligibilityTest,
       ShowChoiceScreenIfTheTimestampPrefIsNotSet) {
  EXPECT_EQ(GetStaticConditions(),
            IfSupported(SearchEngineChoiceScreenConditions::kEligible));
  EXPECT_EQ(GetDynamicConditions(),
            IfSupported(SearchEngineChoiceScreenConditions::kEligible));

  search_engine_choice_service().RecordChoiceMade(
      search_engines::ChoiceMadeLocation::kChoiceScreen,
      &template_url_service());

  EXPECT_EQ(GetStaticConditions(),
            IfSupported(SearchEngineChoiceScreenConditions::kAlreadyCompleted));
  EXPECT_EQ(GetDynamicConditions(),
            IfSupported(SearchEngineChoiceScreenConditions::kAlreadyCompleted));
}

// Test that there is a regional condition controlling eligibility.
TEST_F(SearchEngineChoiceEligibilityTest,
       DoNotShowChoiceScreenIfCountryOutOfScope) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kSearchEngineChoiceCountry, "US");
  EXPECT_EQ(
      GetStaticConditions(),
      IfSupported(SearchEngineChoiceScreenConditions::kNotInRegionalScope));
}

// Test that the choice screen does get displayed even if completed if the
// command line argument for forcing it is set.
TEST_F(SearchEngineChoiceEligibilityTest,
       ShowChoiceScreenWithForceCommandLineFlag) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kForceSearchEngineChoiceScreen);
  search_engines::MarkSearchEngineChoiceCompletedForTesting(*pref_service());

  // `kForceSearchEngineChoiceScreen` is checked during the creation of
  // `search_engine_choice_service` which already happens during test set up.
  InitService({.force_reset = true});

  EXPECT_EQ(GetStaticConditions(),
            IfSupported(SearchEngineChoiceScreenConditions::kEligible));
  EXPECT_EQ(GetDynamicConditions(),
            IfSupported(SearchEngineChoiceScreenConditions::kEligible));
}

TEST_F(SearchEngineChoiceEligibilityTest,
       ShowChoiceScreenWithForceCommandLineFlag_Counterfactual) {
  search_engines::MarkSearchEngineChoiceCompletedForTesting(*pref_service());

  EXPECT_EQ(GetStaticConditions(),
            IfSupported(SearchEngineChoiceScreenConditions::kAlreadyCompleted));
  EXPECT_EQ(GetDynamicConditions(),
            IfSupported(SearchEngineChoiceScreenConditions::kAlreadyCompleted));
}

// Test that the choice screen does not get displayed if the command line
// argument for disabling it is set.
TEST_F(SearchEngineChoiceEligibilityTest,
       DoNotShowChoiceScreenWithDisableCommandLineFlag) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableSearchEngineChoiceScreen);
  EXPECT_EQ(
      GetStaticConditions(),
      IfSupported(SearchEngineChoiceScreenConditions::kFeatureSuppressed));
}

TEST_F(SearchEngineChoiceEligibilityTest, ChoiceScreenConditions_SkipFor3p) {
  // First, check the state with Google as the default search engine
  ASSERT_TRUE(
      template_url_service().GetDefaultSearchProvider()->prepopulate_id() ==
      TemplateURLPrepopulateData::google.id);

  EXPECT_EQ(GetStaticConditions(),
            IfSupported(SearchEngineChoiceScreenConditions::kEligible));
  EXPECT_EQ(GetDynamicConditions(),
            IfSupported(SearchEngineChoiceScreenConditions::kEligible));

  // Second, check the state after changing the default search engine.

  TemplateURL* template_url = template_url_service().GetTemplateURLForKeyword(
      TemplateURLPrepopulateData::bing.keyword);
  ASSERT_TRUE(template_url);
  template_url_service().SetUserSelectedDefaultSearchProvider(template_url);

  EXPECT_EQ(GetStaticConditions(),
            IfSupported(SearchEngineChoiceScreenConditions::kEligible));
  EXPECT_EQ(GetDynamicConditions(),
            IfSupported(
                SearchEngineChoiceScreenConditions::kHasNonGoogleSearchEngine));
}

TEST_F(SearchEngineChoiceEligibilityTest,
       DoNotShowChoiceScreenIfUserHasCustomSearchEngineSetAsDefault) {
  // A custom search engine will have a `prepopulate_id` of 0.
  const int kCustomSearchEnginePrepopulateId = 0;
  TemplateURLData template_url_data;
  template_url_data.prepopulate_id = kCustomSearchEnginePrepopulateId;
  template_url_data.SetURL("https://www.example.com/?q={searchTerms}");
  template_url_service().SetUserSelectedDefaultSearchProvider(
      template_url_service().Add(
          std::make_unique<TemplateURL>(template_url_data)));

  EXPECT_EQ(GetStaticConditions(),
            IfSupported(SearchEngineChoiceScreenConditions::kEligible));
  EXPECT_EQ(GetDynamicConditions(),
            IfSupported(
                SearchEngineChoiceScreenConditions::kHasNonGoogleSearchEngine));
}

}  // namespace
