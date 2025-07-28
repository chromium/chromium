// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <variant>

#include "base/command_line.h"
#include "base/containers/fixed_flat_map.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/version_info/version_info.h"
#include "components/country_codes/country_codes.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/policy/policy_constants.h"
#include "components/regional_capabilities/regional_capabilities_country_id.h"
#include "components/regional_capabilities/regional_capabilities_switches.h"
#include "components/search_engines/choice_made_location.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_service.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_service_test_base.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_utils.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/search_engines/search_engines_test_environment.h"
#include "components/search_engines/search_engines_test_util.h"
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
using search_engines::ChoiceMadeLocation;
using search_engines::SearchEngineChoiceScreenConditions;
using search_engines::SearchEngineChoiceWipeReason;
using search_engines::SearchEnginesTestEnvironment;
using search_engines::WipeSearchEngineChoicePrefs;
using TemplateURLPrepopulateData::PrepopulatedEngine;
using ChoiceStatus = search_engines::SearchEngineChoiceService::ChoiceStatus;

struct PersistedState {
  std::string country_code;
  bool was_choice_made;
  raw_ptr<const PrepopulatedEngine> dse;
};

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
  SearchEngineChoiceEligibilityTest()
      : SearchEngineChoiceEligibilityTest(
            /*skip_search_engine_choice_service_init=*/false) {}
  explicit SearchEngineChoiceEligibilityTest(
      bool skip_search_engine_choice_service_init)
      : skip_search_engine_choice_service_init_(
            skip_search_engine_choice_service_init) {}

  ~SearchEngineChoiceEligibilityTest() override { ResetDeps(); }

  void ResetDeps() {
    ResetServices();  // Depends on db_holder, so reset it first.
    keywords_db_holder_.reset();
  }

  void ApplyPersistedState(PersistedState persisted_state) {
    auto* command_line = base::CommandLine::ForCurrentProcess();
    command_line->RemoveSwitch(switches::kSearchEngineChoiceCountry);
    command_line->AppendSwitchASCII(switches::kSearchEngineChoiceCountry,
                                    persisted_state.country_code);

    if (persisted_state.was_choice_made) {
      search_engines::MarkSearchEngineChoiceCompletedForTesting(
          *pref_service());
    }

    FinalizeEnvironmentInit();

    if (persisted_state.dse != nullptr) {
      auto* turl = template_url_service().GetTemplateURLForKeyword(
          persisted_state.dse->keyword);
      ASSERT_TRUE(turl);
      template_url_service().SetUserSelectedDefaultSearchProvider(turl);
    }
  }

  void PopulateLazyFactories(
      SearchEnginesTestEnvironment::ServiceFactories& lazy_factories,
      search_engines::InitServiceArgs args) override {
    search_engines::SearchEngineChoiceServiceTestBase::PopulateLazyFactories(
        lazy_factories, args);
    lazy_factories.template_url_service_factory = base::BindLambdaForTesting(
        [&](SearchEnginesTestEnvironment& environment) {
          if (!keywords_db_holder_) {
            keywords_db_holder_ =
                std::make_unique<KeywordsDatabaseHolder>(task_environment_);
            keywords_db_holder_->Init();
          }

          return std::make_unique<TemplateURLService>(
              environment.pref_service(),
              environment.search_engine_choice_service(),
              environment.prepopulate_data_resolver(),
              std::make_unique<SearchTermsData>(),
              keywords_db_holder_->keyword_web_data,
              /* TemplateURLServiceClient= */ nullptr,
              /* dsp_change_callback= */ base::RepeatingClosure());
        });

    lazy_factories.search_engine_choice_service_factory =
        SearchEnginesTestEnvironment::GetSearchEngineChoiceServiceFactory(
            // Deliberately do not Init the service here! We'll do it explicitly
            // either in the test itself when
            // `skip_search_engine_choice_service_init_` is set, or in
            // `FinalizeEnvironmentInit()` otherwise. This allows reading the
            // choice state from the service without having it process and
            // update this state on construction.
            /*skip_init=*/true,
            /*client_factory=*/base::BindLambdaForTesting([args]() {
              std::unique_ptr<search_engines::SearchEngineChoiceService::Client>
                  client =
                      std::make_unique<FakeSearchEngineChoiceServiceClient>(
                          args.variation_country_id,
                          args.is_profile_eligible_for_dse_guest_propagation,
                          args.restore_detected_in_current_session,
                          args.choice_predates_restore);
              return client;
            }));
  }

  void FinalizeEnvironmentInit() override {
    if (!skip_search_engine_choice_service_init_) {
      search_engine_choice_service().Init();
    }

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
        policy_service(), template_url_service());
  }

 private:
  bool skip_search_engine_choice_service_init_ = false;

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
  std::unique_ptr<KeywordsDatabaseHolder> keywords_db_holder_;
};

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
      ChoiceMadeLocation::kChoiceScreen, &template_url_service());

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
  EXPECT_EQ(
      GetDynamicConditions(),
      IfSupported(SearchEngineChoiceScreenConditions::kHasCustomSearchEngine));
}

// Specs for a multi-run test. Defines changes to on-device prefs, actions on
// services, and expectation checks.
struct Spec {
  struct DeviceStateChanges {
    CountryId country_id;
    bool set_restored;
  };
  struct ServiceStateChanges {
    std::optional<std::variant<int, std::string>> select_dse;
    std::optional<ChoiceMadeLocation> choice_location;
  };
  struct ExpectationsWithServices {
    SearchEngineChoiceScreenConditions static_condition;
    SearchEngineChoiceScreenConditions dynamic_condition;
    int current_dse_prepopulate_id;
  };

  // Changes and checks are executed in declaration order, as listed here.
  struct Run {
    std::optional<DeviceStateChanges> update_device_state;
    std::optional<ChoiceStatus> expect_choice_status_before;
    std::optional<ServiceStateChanges> update_service_state;
    std::optional<ExpectationsWithServices> expect_with_services;
    std::optional<ChoiceStatus> expect_choice_status_after;
  };

  std::string test_name;
  bool restore_feature_enabled;
  std::vector<Run> runs;
};

class SearchEngineChoiceEligibilityOnRestoreTest
    : public SearchEngineChoiceEligibilityTest,
      public testing::WithParamInterface<Spec> {
 public:
  SearchEngineChoiceEligibilityOnRestoreTest()
      : SearchEngineChoiceEligibilityTest(
            /*skip_search_engine_choice_service_init_=*/true) {}
  ~SearchEngineChoiceEligibilityOnRestoreTest() override = default;

  void CheckChoiceStatus(ChoiceStatus expected_choice_status) {
    EXPECT_EQ(
        search_engine_choice_service().EvaluateSearchProviderChoiceForTesting(
            template_url_service()),
        expected_choice_status);
  }

  void ProcessServicesExpectations(
      Spec::ExpectationsWithServices expectations) {
    EXPECT_EQ(GetStaticConditions(),
              IfSupported(expectations.static_condition));
    EXPECT_EQ(GetDynamicConditions(),
              IfSupported(expectations.dynamic_condition));

    EXPECT_EQ(
        template_url_service().GetDefaultSearchProvider()->prepopulate_id(),
        expectations.current_dse_prepopulate_id);
  }

  void UpdateDeviceState(
      std::optional<Spec::DeviceStateChanges> state_changes) {
    bool restore_detected_in_current_session = false;
    if (state_changes.has_value()) {
      if (state_changes->set_restored) {
        restore_detected_in_current_session = true;
        latest_restore_time_ = base::Time::Now();
      }

      if (state_changes->country_id.IsValid()) {
        auto* command_line = base::CommandLine::ForCurrentProcess();
        command_line->RemoveSwitch(switches::kSearchEngineChoiceCountry);
        command_line->AppendSwitchASCII(
            switches::kSearchEngineChoiceCountry,
            state_changes->country_id.CountryCode());
      }
    }

    InitService({
        .force_reset = true,
        .restore_detected_in_current_session =
            restore_detected_in_current_session,
    });

    if (latest_restore_time_.has_value()) {
      static_cast<FakeSearchEngineChoiceServiceClient&>(
          search_engine_choice_service().GetClientForTesting())
          .set_restore_detection_time(latest_restore_time_.value());
    }
  }

  void UpdateServiceState(Spec::ServiceStateChanges state_changes) {
    ASSERT_EQ(state_changes.select_dse.has_value(),
              state_changes.choice_location.has_value());

    // Process the requested DSE selection & choice location.
    if (state_changes.select_dse.has_value()) {
      TemplateURL* t_url;
      if (std::holds_alternative<std::string>(
              state_changes.select_dse.value())) {
        TemplateURLData custom_turl_data;
        custom_turl_data.SetURL(
            std::get<std::string>(state_changes.select_dse.value()));
        t_url = template_url_service().Add(
            std::make_unique<TemplateURL>(custom_turl_data));
      } else {
        int select_id = std::get<int>(state_changes.select_dse.value());
        auto t_urls = template_url_service().GetTemplateURLs();
        if (auto engine_it = std::ranges::find_if(
                t_urls,
                [&](const auto& engine) {
                  return engine->prepopulate_id() == select_id;
                });
            engine_it != t_urls.cend()) {
          t_url = *engine_it;
        }
      }
      ASSERT_TRUE(t_url);

      template_url_service().SetUserSelectedDefaultSearchProvider(
          t_url, *state_changes.choice_location);
    }
  }

  std::optional<base::Time> latest_restore_time_;
};

TEST_P(SearchEngineChoiceEligibilityOnRestoreTest, Run) {
  const Spec& param = GetParam();

  base::test::ScopedFeatureList scoped_feature_list;
  if (param.restore_feature_enabled) {
    scoped_feature_list.InitAndEnableFeature(
        switches::kInvalidateSearchEngineChoiceOnDeviceRestoreDetection);
  } else {
    scoped_feature_list.InitAndDisableFeature(
        switches::kInvalidateSearchEngineChoiceOnDeviceRestoreDetection);
  }

  latest_restore_time_ = std::nullopt;
  for (const auto& current_run : param.runs) {
    ResetServices();

    ASSERT_FALSE(search_engines_test_environment_);
    UpdateDeviceState(current_run.update_device_state);

    if (current_run.expect_choice_status_before.has_value()) {
      CheckChoiceStatus(*current_run.expect_choice_status_before);
    }

    // Done explicitly here, which is why we skip the built-in initialization
    // from the base fixture.
    search_engine_choice_service().Init();

    if (current_run.update_service_state.has_value()) {
      UpdateServiceState(*current_run.update_service_state);
    }

    if (current_run.expect_with_services.has_value()) {
      ProcessServicesExpectations(*current_run.expect_with_services);
    }

    if (current_run.expect_choice_status_after.has_value()) {
      CheckChoiceStatus(*current_run.expect_choice_status_after);
    }
  }
}

INSTANTIATE_TEST_SUITE_P(
    ,
    SearchEngineChoiceEligibilityOnRestoreTest,
    ::testing::ValuesIn(
        {Spec{.test_name = "1p",
              .restore_feature_enabled = true,
              .runs =
                  {
                      // Sets up Chrome as running in France, and having
                      // selected Google on the choice screen
                      {
                          .update_device_state =
                              Spec::DeviceStateChanges{
                                  .country_id = CountryId("FR"),
                              },
                          .update_service_state =
                              Spec::ServiceStateChanges{
                                  .select_dse =
                                      TemplateURLPrepopulateData::google.id,
                                  .choice_location =
                                      ChoiceMadeLocation::kChoiceScreen,
                              },
                          .expect_choice_status_after = ChoiceStatus::kValid,
                      },
                      // Simulates the device being restored, and its detection
                      // in this run. The client becomes eligible again for a
                      // choice screen, the old choice is marked invalid.
                      {
                          .update_device_state =
                              Spec::DeviceStateChanges{
                                  .set_restored = true,
                              },
                          .expect_choice_status_before =
                              ChoiceStatus::kFromRestoredDevice,
                          .expect_with_services =
                              Spec::ExpectationsWithServices{
                                  .static_condition =
                                      SearchEngineChoiceScreenConditions::
                                          kEligible,
                                  .dynamic_condition =
                                      SearchEngineChoiceScreenConditions::
                                          kEligible,
                                  .current_dse_prepopulate_id =
                                      TemplateURLPrepopulateData::google.id,
                              },
                          .expect_choice_status_after =
                              ChoiceStatus::kFromRestoredDevice,
                      },
                  }},
         Spec{
             .test_name = "1pNoRestoreDetection",
             .restore_feature_enabled = false,
             .runs =
                 {
                     // Sets up Chrome as running in France, and having
                     // selected Google on the choice screen
                     {
                         .update_device_state =
                             Spec::DeviceStateChanges{
                                 .country_id = CountryId("FR"),
                             },
                         .update_service_state =
                             Spec::ServiceStateChanges{
                                 .select_dse =
                                     TemplateURLPrepopulateData::google.id,
                                 .choice_location =
                                     ChoiceMadeLocation::kChoiceScreen,
                             },
                     },
                     // Simulates the device being restored. Detection is
                     // disabled, so nothing happens, the client stays
                     // ineligible because already completed.
                     {
                         .update_device_state =
                             Spec::DeviceStateChanges{
                                 .set_restored = true,
                             },
                         .expect_choice_status_before = ChoiceStatus::kValid,
                         .expect_with_services =
                             Spec::ExpectationsWithServices{
                                 .static_condition =
                                     SearchEngineChoiceScreenConditions::
                                         kAlreadyCompleted,
                                 .dynamic_condition =
                                     SearchEngineChoiceScreenConditions::
                                         kAlreadyCompleted,
                                 .current_dse_prepopulate_id =
                                     TemplateURLPrepopulateData::google.id,
                             },
                         .expect_choice_status_after = ChoiceStatus::kValid,
                     },
                 },
         },
         Spec{.test_name = "3p",
              .restore_feature_enabled = true,
              .runs =
                  {
                      // Sets up Chrome as running in France, and having
                      // selected a 3P search engine on the choice screen
                      {
                          .update_device_state =
                              Spec::DeviceStateChanges{
                                  .country_id = CountryId("FR"),
                              },
                          .update_service_state =
                              Spec::ServiceStateChanges{
                                  .select_dse =
                                      TemplateURLPrepopulateData::bing.id,
                                  .choice_location =
                                      ChoiceMadeLocation::kChoiceScreen,
                              },
                          .expect_with_services =
                              Spec::ExpectationsWithServices{
                                  .static_condition =
                                      SearchEngineChoiceScreenConditions::
                                          kAlreadyCompleted,
                                  .dynamic_condition =
                                      SearchEngineChoiceScreenConditions::
                                          kAlreadyCompleted,
                                  .current_dse_prepopulate_id =
                                      TemplateURLPrepopulateData::bing.id,
                              },
                          .expect_choice_status_after = ChoiceStatus::kValid,
                      },
                      // Simulates the device being restored, and its detection
                      // in this run. The client becomes eligible again for a
                      // choice screen, the old selection is marked invalid.
                      {
                          .update_device_state =
                              Spec::DeviceStateChanges{
                                  .set_restored = true,
                              },
                          .expect_with_services =
                              Spec::ExpectationsWithServices{
                                  .static_condition =
                                      SearchEngineChoiceScreenConditions::
                                          kEligible,
                                  .dynamic_condition =
                                      SearchEngineChoiceScreenConditions::
                                          kEligible,
                                  .current_dse_prepopulate_id =
                                      TemplateURLPrepopulateData::bing.id,
                              },
                          .expect_choice_status_after =
                              ChoiceStatus::kFromRestoredDevice,
                      },
                      // Select a different 3P DSE on the choice screen, it
                      // restores the selection state to the usual
                      // (completed, choice valid).
                      {
                          .update_service_state =
                              Spec::ServiceStateChanges{
                                  .select_dse =
                                      TemplateURLPrepopulateData::duckduckgo.id,
                                  .choice_location =
                                      ChoiceMadeLocation::kChoiceScreen,
                              },
                          .expect_with_services =
                              Spec::ExpectationsWithServices{
                                  .static_condition =
                                      SearchEngineChoiceScreenConditions::
                                          kAlreadyCompleted,
                                  .dynamic_condition =
                                      SearchEngineChoiceScreenConditions::
                                          kAlreadyCompleted,
                                  .current_dse_prepopulate_id =
                                      TemplateURLPrepopulateData::duckduckgo.id,
                              },
                          .expect_choice_status_after = ChoiceStatus::kValid,
                      },
                  }},
         Spec{
             .test_name = "3pNoRestoreDetection",
             .restore_feature_enabled = false,
             .runs =
                 {
                     // Sets up Chrome as running in France, and having
                     // selected a 3P search engine on the choice screen
                     {
                         .update_device_state =
                             Spec::DeviceStateChanges{
                                 .country_id = CountryId("FR"),
                             },
                         .update_service_state =
                             Spec::ServiceStateChanges{

                                 .select_dse =
                                     TemplateURLPrepopulateData::bing.id,
                                 .choice_location =
                                     ChoiceMadeLocation::kChoiceScreen,
                             },
                         .expect_with_services =
                             Spec::ExpectationsWithServices{
                                 .static_condition =
                                     SearchEngineChoiceScreenConditions::
                                         kAlreadyCompleted,
                                 .dynamic_condition =
                                     SearchEngineChoiceScreenConditions::
                                         kAlreadyCompleted,
                                 .current_dse_prepopulate_id =
                                     TemplateURLPrepopulateData::bing.id,
                             },
                         .expect_choice_status_after = ChoiceStatus::kValid,
                     },
                     // Simulates the device being restored. Detection is
                     // disabled, so nothing happens, the client stays
                     // ineligible because already completed.
                     {
                         .update_device_state =
                             Spec::DeviceStateChanges{
                                 .set_restored = true,
                             },
                         .expect_with_services =
                             Spec::ExpectationsWithServices{
                                 .static_condition =
                                     SearchEngineChoiceScreenConditions::
                                         kAlreadyCompleted,
                                 .dynamic_condition =
                                     SearchEngineChoiceScreenConditions::
                                         kAlreadyCompleted,
                                 .current_dse_prepopulate_id =
                                     TemplateURLPrepopulateData::bing.id,
                             },
                         .expect_choice_status_after = ChoiceStatus::kValid,
                     },
                 },
         },
         Spec{
             .test_name = "custom",
             .restore_feature_enabled = true,
             .runs =
                 {
                     // Sets up Chrome as running in France, and having selected
                     // a custom search engine from the settings.
                     {
                         .update_device_state =
                             Spec::DeviceStateChanges{
                                 .country_id = CountryId("FR"),
                             },
                         .update_service_state =
                             Spec::ServiceStateChanges{
                                 .select_dse =
                                     "https://www.example.com/?q={searchTerms}",
                                 .choice_location =
                                     ChoiceMadeLocation::kSearchEngineSettings,

                             },
                         .expect_with_services =
                             Spec::ExpectationsWithServices{
                                 .static_condition =
                                     SearchEngineChoiceScreenConditions::
                                         kAlreadyCompleted,
                                 .dynamic_condition =
                                     SearchEngineChoiceScreenConditions::
                                         kAlreadyCompleted,
                                 .current_dse_prepopulate_id = 0,
                             },
                         .expect_choice_status_after = ChoiceStatus::kValid,
                     },
                     // Simulates the device being restored, and its detection
                     // in this run. The old selection is marked invalid, but
                     // since it's a custom search engine, we can't reprompt
                     // over it.
                     {
                         .update_device_state =
                             Spec::DeviceStateChanges{
                                 .set_restored = true,
                             },
                         .expect_choice_status_before =
                             ChoiceStatus::kCurrentIsNotPrepopulated,
                         .expect_with_services =
                             Spec::ExpectationsWithServices{
                                 .static_condition =
                                     SearchEngineChoiceScreenConditions::
                                         kEligible,
                                 .dynamic_condition =
                                     SearchEngineChoiceScreenConditions::
                                         kHasCustomSearchEngine,
                                 .current_dse_prepopulate_id = 0,
                             },
                         .expect_choice_status_after =
                             ChoiceStatus::kCurrentIsNotPrepopulated,
                     },
                     // Simulates the DSE being reset to Google outside of a
                     // user interface Not really sure how exactly that can
                     // happen, but we also use this made up flow to
                     // approximate things like a policy being lifted for
                     // example. Not having a custom DSE active makes the
                     // profile eligible for the choice screen. The non-UI DSE
                     // change here should not affect the post-restore
                     // invalidity flag.
                     {
                         .update_service_state =
                             Spec::ServiceStateChanges{
                                 .select_dse =
                                     TemplateURLPrepopulateData::google.id,
                                 .choice_location = ChoiceMadeLocation::kOther,
                             },
                         .expect_with_services =
                             Spec::ExpectationsWithServices{
                                 .static_condition =
                                     SearchEngineChoiceScreenConditions::
                                         kEligible,
                                 .dynamic_condition =
                                     SearchEngineChoiceScreenConditions::
                                         kEligible,
                                 .current_dse_prepopulate_id =
                                     TemplateURLPrepopulateData::google.id,
                             },
                         .expect_choice_status_after =
                             ChoiceStatus::kFromRestoredDevice,
                     },
                     // Select an engine on the choice screen, it restores the
                     // selection state to the usual (completed, choice valid).
                     {
                         .update_service_state =
                             Spec::ServiceStateChanges{
                                 .select_dse =
                                     TemplateURLPrepopulateData::google.id,
                                 .choice_location =
                                     ChoiceMadeLocation::kChoiceScreen,
                             },
                         .expect_with_services =
                             Spec::ExpectationsWithServices{
                                 .static_condition =
                                     SearchEngineChoiceScreenConditions::
                                         kAlreadyCompleted,
                                 .dynamic_condition =
                                     SearchEngineChoiceScreenConditions::
                                         kAlreadyCompleted,
                                 .current_dse_prepopulate_id =
                                     TemplateURLPrepopulateData::google.id,
                             },
                         .expect_choice_status_after = ChoiceStatus::kValid,
                     },
                 },
         },
         Spec{
             .test_name = "customGoogle",
             .restore_feature_enabled = true,
             .runs =
                 {
                     // Sets up Chrome as running in France, and having
                     // selected a custom search engine from the settings.
                     {
                         .update_device_state =
                             Spec::DeviceStateChanges{
                                 .country_id = CountryId("FR"),
                             },
                         .update_service_state =
                             Spec::ServiceStateChanges{
                                 .select_dse =
                                     "https://google.fr/maps?q={searchTerms}",
                                 .choice_location =
                                     ChoiceMadeLocation::kSearchEngineSettings,

                             },
                         .expect_with_services =
                             Spec::ExpectationsWithServices{
                                 .static_condition =
                                     SearchEngineChoiceScreenConditions::
                                         kAlreadyCompleted,
                                 .dynamic_condition =
                                     SearchEngineChoiceScreenConditions::
                                         kAlreadyCompleted,
                                 .current_dse_prepopulate_id = 0,
                             },
                     },
                     // Simulates the device being restored, and its detection
                     // in this run. The old selection is marked invalid, but
                     // since it's a custom search engine, we can't reprompt
                     // over it.
                     {
                         .update_device_state =
                             Spec::DeviceStateChanges{
                                 .set_restored = true,
                             },
                         .expect_choice_status_before =
                             ChoiceStatus::kCurrentIsNotPrepopulated,
                         .expect_with_services =
                             Spec::ExpectationsWithServices{
                                 .static_condition =
                                     SearchEngineChoiceScreenConditions::
                                         kEligible,
                                 .dynamic_condition =
                                     SearchEngineChoiceScreenConditions::
                                         kHasCustomSearchEngine,
                                 .current_dse_prepopulate_id = 0,
                             },
                         .expect_choice_status_after =
                             ChoiceStatus::kCurrentIsNotPrepopulated,
                     },
                     // Simulates the DSE being reset to Google outside of a
                     // user interface Not really sure how exactly that can
                     // happen, but we also use this made up flow to
                     // approximate things like a policy being lifted for
                     // example. Not having a custom DSE active makes the
                     // profile eligible for the choice screen. The non-UI DSE
                     // change here should not affect the post-restore
                     // invalidity flag.
                     {
                         .update_service_state =
                             Spec::ServiceStateChanges{
                                 .select_dse =
                                     TemplateURLPrepopulateData::google.id,
                                 .choice_location = ChoiceMadeLocation::kOther,
                             },
                         .expect_with_services =
                             Spec::ExpectationsWithServices{
                                 .static_condition =
                                     SearchEngineChoiceScreenConditions::
                                         kEligible,
                                 .dynamic_condition =
                                     SearchEngineChoiceScreenConditions::
                                         kEligible,
                                 .current_dse_prepopulate_id =
                                     TemplateURLPrepopulateData::google.id,
                             },
                         .expect_choice_status_after =
                             ChoiceStatus::kFromRestoredDevice,
                     },
                     // Select an engine on the choice screen, it restores the
                     // selection state to the usual (completed, choice valid).
                     {
                         .update_service_state =
                             Spec::ServiceStateChanges{
                                 .select_dse =
                                     TemplateURLPrepopulateData::google.id,
                                 .choice_location =
                                     ChoiceMadeLocation::kChoiceScreen,
                             },
                         .expect_with_services =
                             Spec::ExpectationsWithServices{
                                 .static_condition =
                                     SearchEngineChoiceScreenConditions::
                                         kAlreadyCompleted,
                                 .dynamic_condition =
                                     SearchEngineChoiceScreenConditions::
                                         kAlreadyCompleted,
                                 .current_dse_prepopulate_id =
                                     TemplateURLPrepopulateData::google.id,
                             },
                         .expect_choice_status_after = ChoiceStatus::kValid,
                     },
                 },
         }}),
    [](const testing::TestParamInfo<Spec>& info) {
      return info.param.test_name;
    });

}  // namespace
