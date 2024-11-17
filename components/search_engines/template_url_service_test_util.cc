// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/template_url_service_test_util.h"

#include <memory>

#include "base/command_line.h"
#include "components/country_codes/country_codes.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/search_engines/keyword_table.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_service.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_client.h"
#include "components/webdata/common/web_database_service.h"

void RegisterPrefsForTemplateURLService(
    user_prefs::PrefRegistrySyncable* registry) {
  TemplateURLService::RegisterProfilePrefs(registry);
  TemplateURLPrepopulateData::RegisterProfilePrefs(registry);
  DefaultSearchManager::RegisterProfilePrefs(registry);
}

// -- TemplateURLServiceLoadWaiter --------------------------------------------

TemplateURLServiceLoadWaiter::TemplateURLServiceLoadWaiter() = default;
TemplateURLServiceLoadWaiter::~TemplateURLServiceLoadWaiter() = default;

void TemplateURLServiceLoadWaiter::WaitForLoadComplete(
    TemplateURLService& template_url_service) {
  ASSERT_TRUE(!scoped_template_url_service_observation_.IsObserving());

  if (template_url_service.loaded()) {
    return;
  }
  scoped_template_url_service_observation_.Observe(&template_url_service);
  run_loop_.Run();
}

void TemplateURLServiceLoadWaiter::OnTemplateURLServiceChanged() {
  UpdateStatus();
}

void TemplateURLServiceLoadWaiter::OnTemplateURLServiceShuttingDown() {
  // Added for correctness. Not needed for the classic use cases where we wait
  // for the service to finish loading after creation. But in case a test runs
  // into some issue or a service is destroyed, this is intended to not keep
  // blocking and risk hiding the underlying issue.
  UpdateStatus(/*force_quit=*/true);
}

void TemplateURLServiceLoadWaiter::UpdateStatus(bool force_quit) {
  ASSERT_TRUE(scoped_template_url_service_observation_.IsObserving());
  if (force_quit ||
      scoped_template_url_service_observation_.GetSource()->loaded()) {
    scoped_template_url_service_observation_.Reset();
    run_loop_.Quit();
  }
}

// -- TemplateURLServiceUnitTestBase ------------------------------------------

TemplateURLServiceUnitTestBase::TemplateURLServiceUnitTestBase() = default;
TemplateURLServiceUnitTestBase::~TemplateURLServiceUnitTestBase() = default;

void TemplateURLServiceUnitTestBase::SetUp() {
  RegisterPrefsForTemplateURLService(pref_service_.registry());
  local_state_.registry()->RegisterBooleanPref(
      metrics::prefs::kMetricsReportingEnabled, true);
  // Bypass the country checks.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kSearchEngineChoiceCountry,
      switches::kDefaultListCountryOverride);

  search_engine_choice_service_ =
      std::make_unique<search_engines::SearchEngineChoiceService>(
          pref_service_, &local_state_,
#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
          /*is_profile_eligible_for_dse_guest_propagation=*/false,
#endif
          country_codes::kCountryIDUnknown);

  template_url_service_ = CreateService();
}

std::unique_ptr<TemplateURLService>
TemplateURLServiceUnitTestBase::CreateService() {
  return std::make_unique<TemplateURLService>(
      pref_service_, *search_engine_choice_service_,
      std::make_unique<SearchTermsData>(), nullptr /* KeywordWebDataService */,
      nullptr /* TemplateURLServiceClient */, base::RepeatingClosure()
#if BUILDFLAG(IS_CHROMEOS_LACROS)
                                                  ,
      false
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  );
}

// -- LoadedTemplateURLServiceUnitTestBase ------------------------------------

LoadedTemplateURLServiceUnitTestBase::LoadedTemplateURLServiceUnitTestBase()
    : os_crypt_(os_crypt_async::GetTestOSCryptAsyncForTesting()) {}
LoadedTemplateURLServiceUnitTestBase::~LoadedTemplateURLServiceUnitTestBase() =
    default;

std::unique_ptr<TemplateURLService>
LoadedTemplateURLServiceUnitTestBase::CreateService() {
  CHECK(!database_);
  CHECK(!keyword_data_service_);

  auto task_runner = task_environment.GetMainThreadTaskRunner();

  database_ = base::MakeRefCounted<WebDatabaseService>(
      base::FilePath(WebDatabase::kInMemoryPath),
      /*ui_task_runner=*/task_runner,
      /*db_task_runner=*/task_runner);
  database_->AddTable(std::make_unique<KeywordTable>());
  database_->LoadDatabase(os_crypt_.get());

  keyword_data_service_ =
      base::MakeRefCounted<KeywordWebDataService>(database_, task_runner);
  keyword_data_service_->Init(base::DoNothing());

  auto template_url_service = std::make_unique<TemplateURLService>(
      pref_service(), search_engine_choice_service(),
      std::make_unique<SearchTermsData>(), keyword_data_service_,
      nullptr /* TemplateURLServiceClient */, base::RepeatingClosure()
#if BUILDFLAG(IS_CHROMEOS_LACROS)
                                                  ,
      false
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  );

  return template_url_service;
}

void LoadedTemplateURLServiceUnitTestBase::SetUp() {
  TemplateURLServiceUnitTestBase::SetUp();

  ASSERT_FALSE(template_url_service().loaded());

  template_url_service().Load();
  template_url_service_load_waiter_.WaitForLoadComplete(template_url_service());

  ASSERT_EQ(GetKeywordTemplateURLs().size(),
            TemplateURLPrepopulateData::GetDefaultPrepopulatedEngines().size());
}

void LoadedTemplateURLServiceUnitTestBase::TearDown() {
  template_url_service().Shutdown();

  keyword_data_service_->ShutdownOnUISequence();
  database_->ShutdownDatabase();
}

// Same as `TemplateURLService::GetTemplateURLs()`, but removes the starter
// pack entries.
TemplateURLService::TemplateURLVector
LoadedTemplateURLServiceUnitTestBase::GetKeywordTemplateURLs() {
  TemplateURLService::TemplateURLVector turls =
      template_url_service().GetTemplateURLs();
  turls.erase(base::ranges::remove_if(turls,
                                      [](const TemplateURL* turl) {
                                        return turl->starter_pack_id() != 0;
                                      }),
              turls.end());
  return turls;
}

// Returns `TemplateURLService::GetTemplateURLs()`, filtering to only entries
// that have `keyword` as their keyword. In general it should return a single
// elements, but in some cases there may be more.
TemplateURLService::TemplateURLVector
LoadedTemplateURLServiceUnitTestBase::GetTemplateURLsMatchingKeyword(
    std::u16string keyword) {
  TemplateURLService::TemplateURLVector matching_turls;
  for (const auto& turl : template_url_service().GetTemplateURLs()) {
    if (turl->keyword() == keyword) {
      matching_turls.push_back(turl);
    }
  }
  return matching_turls;
}
