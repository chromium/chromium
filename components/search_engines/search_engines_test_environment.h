// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINES_TEST_ENVIRONMENT_H_
#define COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINES_TEST_ENVIRONMENT_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_span.h"
#include "components/prefs/testing_pref_service.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_service.h"
#include "components/search_engines/template_url_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"

namespace regional_capabilities {
class RegionalCapabilitiesService;
}

namespace TemplateURLPrepopulateData {
class Resolver;
}

namespace search_engines {

// Test helper that makes it easier to create a `TemplateURLService` and a
// `SearchEngineChoiceService`. The caller should not worry about the
// dependencies needed to create those classes.
// `pref_service` and `local_state` can be passed by the caller using `Deps`. It
// will be the caller's responsibility to make sure the incoming pref services
// have the needed registrations.
// The creation of the various services involved with `TemplateURLService` can
// be controlled by passing a custom `ServiceFactories`.
//
// NOTE: SearchEnginesTestEnvironment requires that tests have a properly set up
// task environment. If your test doesn't already have one, use a
// base::test::TaskEnvironment instance variable to fulfill this
// requirement.
class SearchEnginesTestEnvironment {
 public:
  struct Deps {
    raw_ptr<sync_preferences::TestingPrefServiceSyncable> pref_service =
        nullptr;
    raw_ptr<TestingPrefServiceSimple> local_state = nullptr;
    base::raw_span<const TemplateURLService::Initializer>
        template_url_service_initializer;
  };

  template <typename T>
  using ServiceFactory = base::RepeatingCallback<std::unique_ptr<T>(
      SearchEnginesTestEnvironment& self)>;

  struct ServiceFactories {
    ServiceFactory<regional_capabilities::RegionalCapabilitiesService>
        regional_capabilities_service_factory;
    ServiceFactory<SearchEngineChoiceService>
        search_engine_choice_service_factory;
    ServiceFactory<TemplateURLService> template_url_service_factory;

    ServiceFactories();
    ServiceFactories(const ServiceFactories& other);
    ServiceFactories& operator=(const ServiceFactories& other);
    ~ServiceFactories();
  };

  static ServiceFactory<SearchEngineChoiceService>
  GetSearchEngineChoiceServiceFactory(
      bool skip_init = false,
      base::RepeatingCallback<
          std::unique_ptr<SearchEngineChoiceService::Client>()> client_factory =
          base::RepeatingCallback<
              std::unique_ptr<SearchEngineChoiceService::Client>()>());

  explicit SearchEnginesTestEnvironment(
      const Deps& deps = {/*pref_service=*/nullptr,
                          /*local_state=*/nullptr,
                          /*template_url_service_initializer=*/{}},
      const ServiceFactories& service_factories = ServiceFactories());
  SearchEnginesTestEnvironment(const SearchEnginesTestEnvironment&) = delete;
  SearchEnginesTestEnvironment& operator=(const SearchEnginesTestEnvironment&) =
      delete;

  ~SearchEnginesTestEnvironment();

  void Shutdown();

  sync_preferences::TestingPrefServiceSyncable& pref_service();
  sync_preferences::TestingPrefServiceSyncable& pref_service() const;

  TestingPrefServiceSimple& local_state();

  regional_capabilities::RegionalCapabilitiesService&
  regional_capabilities_service();

  TemplateURLPrepopulateData::Resolver& prepopulate_data_resolver();

  policy::ManagementService& management_service();

  SearchEngineChoiceService& search_engine_choice_service();

  // Guaranteed to be non-null unless `ReleaseTemplateURLService` has been
  // called.
  TemplateURLService* template_url_service();

  // As the services are lazily created, this might unexpectedly return null
  // when it's the first access to this service. Call the non-const version of
  // the function if forcing the service creation earlier is needed.
  const TemplateURLService* template_url_service() const;

  signin::IdentityTestEnvironment& identity_test_env() {
    return identity_test_env_;
  }

  [[nodiscard]] std::unique_ptr<TemplateURLService> ReleaseTemplateURLService();

 private:
  signin::IdentityTestEnvironment identity_test_env_;

  // Created when `Deps::local_state` is not provided.
  // Don't access it directly, prefer using `local_state_` instead.
  std::unique_ptr<TestingPrefServiceSimple> owned_local_state_;
  raw_ptr<TestingPrefServiceSimple> local_state_;

  // Created when `Deps::pref_service` is not provided.
  // Don't access it directly, prefer using `pref_service_` instead.
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable>
      owned_pref_service_;
  raw_ptr<sync_preferences::TestingPrefServiceSyncable> pref_service_;

  const ServiceFactories service_factories_overrides_;
  const ServiceFactories default_factories_;

  // Services below have dependencies between each other, they must be kept
  // in order to ensure destruction correctness.
  std::unique_ptr<regional_capabilities::RegionalCapabilitiesService>
      regional_capabilities_service_;
  std::unique_ptr<TemplateURLPrepopulateData::Resolver>
      prepopulate_data_resolver_;
  std::unique_ptr<policy::ManagementService> management_service_;
  std::unique_ptr<SearchEngineChoiceService> search_engine_choice_service_;
  std::unique_ptr<TemplateURLService> template_url_service_;
  bool released_template_url_service_ = false;
};

}  // namespace search_engines

#endif  // COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINES_TEST_ENVIRONMENT_H_
