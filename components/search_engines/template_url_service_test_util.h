// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_SERVICE_TEST_UTIL_H_
#define COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_SERVICE_TEST_UTIL_H_

#include "base/scoped_observation.h"
#include "base/test/task_environment.h"
#include "components/prefs/testing_pref_service.h"
#include "components/search_engines/template_url_prepopulate_data_resolver.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_observer.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

class KeywordWebDataService;
class WebDatabaseService;

namespace base {
class RunLoop;
}
namespace os_crypt_async {
class OSCryptAsync;
}
namespace policy {
class ManagementService;
}
namespace regional_capabilities {
class RegionalCapabilitiesService;
}
namespace search_engines {
class SearchEngineChoiceService;
}

void RegisterPrefsForTemplateURLService(
    user_prefs::PrefRegistrySyncable* registry);

// One-shot observer that blocks until `template_url_service` is done loading.
class TemplateURLServiceLoadWaiter : public TemplateURLServiceObserver {
 public:
  TemplateURLServiceLoadWaiter();
  ~TemplateURLServiceLoadWaiter() override;

  void WaitForLoadComplete(TemplateURLService& template_url_service);

  void OnTemplateURLServiceChanged() override;

  void OnTemplateURLServiceShuttingDown() override;

 private:
  void UpdateStatus(bool force_quit = false);

  base::RunLoop run_loop_;

  base::ScopedObservation<TemplateURLService, TemplateURLServiceObserver>
      scoped_template_url_service_observation_{this};
};

class TemplateURLServiceUnitTestBase : public testing::Test {
 public:
  TemplateURLServiceUnitTestBase();
  ~TemplateURLServiceUnitTestBase() override;

  void SetUp() override;

  PrefService& pref_service() { return pref_service_; }

  regional_capabilities::RegionalCapabilitiesService&
  regional_capabilities_service() {
    return *regional_capabilities_service_.get();
  }

  TemplateURLPrepopulateData::Resolver& prepopulate_data_resolver() {
    return *prepopulate_data_resolver_.get();
  }

  search_engines::SearchEngineChoiceService& search_engine_choice_service() {
    return *search_engine_choice_service_.get();
  }

  TemplateURLService& template_url_service() {
    return *template_url_service_.get();
  }

 protected:
  virtual std::unique_ptr<TemplateURLService> CreateService();

  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::MainThreadType::UI};

 private:
  signin::IdentityTestEnvironment identity_test_env_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  TestingPrefServiceSimple local_state_;
  std::unique_ptr<regional_capabilities::RegionalCapabilitiesService>
      regional_capabilities_service_;
  std::unique_ptr<TemplateURLPrepopulateData::Resolver>
      prepopulate_data_resolver_;
  std::unique_ptr<policy::ManagementService> management_service_;
  std::unique_ptr<search_engines::SearchEngineChoiceService>
      search_engine_choice_service_;
  std::unique_ptr<TemplateURLService> template_url_service_;
};

class LoadedTemplateURLServiceUnitTestBase
    : public TemplateURLServiceUnitTestBase {
 public:
  LoadedTemplateURLServiceUnitTestBase();
  ~LoadedTemplateURLServiceUnitTestBase() override;

 protected:
  std::unique_ptr<TemplateURLService> CreateService() override;

  void SetUp() override;
  void TearDown() override;

  // Same as `TemplateURLService::GetTemplateURLs()`, but removes the starter
  // pack entries.
  TemplateURLService::TemplateURLVector GetKeywordTemplateURLs();

  // Returns `TemplateURLService::GetTemplateURLs()`, filtering to only entries
  // that have `keyword` as their keyword. In general it should return a single
  // elements, but in some cases there may be more.
  TemplateURLService::TemplateURLVector GetTemplateURLsMatchingKeyword(
      std::u16string keyword);

 private:
  scoped_refptr<WebDatabaseService> database_;
  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt_;
  scoped_refptr<KeywordWebDataService> keyword_data_service_;
  TemplateURLServiceLoadWaiter template_url_service_load_waiter_;
};

#endif  // COMPONENTS_SEARCH_ENGINES_TEMPLATE_URL_SERVICE_TEST_UTIL_H_
