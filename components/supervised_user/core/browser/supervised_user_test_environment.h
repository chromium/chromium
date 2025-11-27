// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_TEST_ENVIRONMENT_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_TEST_ENVIRONMENT_H_

#include <map>
#include <memory>
#include <string_view>

#include "base/memory/ref_counted.h"
#include "components/prefs/pref_notifier_impl.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_store.h"
#include "components/safe_search_api/fake_url_checker_client.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/supervised_user/core/browser/supervised_user_content_filters_service.h"
#include "components/supervised_user/core/browser/supervised_user_metrics_service.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "components/supervised_user/core/browser/supervised_user_settings_service.h"
#include "components/supervised_user/test_support/supervised_user_url_filter_test_utils.h"
#include "components/sync/test/mock_sync_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "services/network/test/test_url_loader_factory.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/supervised_user/core/browser/android/content_filters_observer_bridge.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace supervised_user {

// Handy set of initial states of supervision stack, to preset before testing.
enum class InitialSupervisionState : int {
  // Default mode, no supervision, no content filters at startup.
  kUnsupervised,
  // Enable family link, and use defaults.
  kFamilyLinkDefault,
  // Enable family link and set specific initial settings.
  kFamilyLinkAllowAllSites,
  kFamilyLinkTryToBlockMatureSites,
  kFamilyLinkCertainSites,
  // Local supervision startup states.
  kSupervisedWithAllContentFilters,
};

// Launches the service from empty settings, typically during context
// initialization.
SupervisedUserSettingsService* InitializeSettingsServiceForTesting(
    SupervisedUserSettingsService* settings_service);

// Prepares a pref service component for use in test.
scoped_refptr<TestingPrefStore> CreateTestingPrefStore(
    SupervisedUserSettingsService* settings_service,
    SupervisedUserContentFiltersService* content_filters_service);

// Pref service exposed by this environment has the supervised user pref store
// configured.
class SupervisedUserPrefStoreTestEnvironment {
 public:
  SupervisedUserPrefStoreTestEnvironment();
  SupervisedUserPrefStoreTestEnvironment(
      const SupervisedUserPrefStoreTestEnvironment&) = delete;
  SupervisedUserPrefStoreTestEnvironment& operator=(
      const SupervisedUserPrefStoreTestEnvironment&) = delete;
  ~SupervisedUserPrefStoreTestEnvironment();

  PrefService* pref_service();
  SupervisedUserSettingsService* settings_service();
  SupervisedUserContentFiltersService* content_filters_service();

  void Shutdown();

  // Sets initial values in components like pref service and content filters
  // before services are created.
  void ConfigureInitialValues(InitialSupervisionState initial_state);

 private:
  SupervisedUserSettingsService settings_service_;
  SupervisedUserContentFiltersService content_filters_service_;

  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable>
      syncable_pref_service_ =
          std::make_unique<sync_preferences::TestingPrefServiceSyncable>(
              /*managed_prefs=*/base::MakeRefCounted<TestingPrefStore>(),
              /*supervised_user_prefs=*/
              CreateTestingPrefStore(
                  InitializeSettingsServiceForTesting(&settings_service_),
                  &content_filters_service_),
              /*extension_prefs=*/base::MakeRefCounted<TestingPrefStore>(),
              /*user_prefs=*/base::MakeRefCounted<TestingPrefStore>(),
              /*recommended_prefs=*/base::MakeRefCounted<TestingPrefStore>(),
              base::MakeRefCounted<user_prefs::PrefRegistrySyncable>(),
              std::make_unique<PrefNotifierImpl>());
};

class SupervisedUserMetricsServiceExtensionDelegateFake
    : public SupervisedUserMetricsService::
          SupervisedUserMetricsServiceExtensionDelegate {
  bool RecordExtensionsMetrics() override;
};

class MetricsServiceAccessorDelegateMock
    : public SupervisedUserMetricsService::MetricsServiceAccessorDelegate {
 public:
  MetricsServiceAccessorDelegateMock();
  ~MetricsServiceAccessorDelegateMock() override;
  MOCK_METHOD(void,
              RegisterSyntheticFieldTrial,
              (std::string_view trial_name, std::string_view group_name),
              (override));
};

#if BUILDFLAG(IS_ANDROID)
// Fake implementation of ContentFiltersObserverBridge for testing. Imitates
// events that would normally be produced by the Android's secure settings
// (which store content filter settings). Content bridge is initialized with
// "disabled" setting.
class FakeContentFiltersObserverBridge final
    : public ContentFiltersObserverBridge {
 public:
  // Matching constructor of ContentFiltersObserverBridge. Setting the initial
  // value to true helps to test scenarios when the browser is started with the
  // setting already enabled.
  FakeContentFiltersObserverBridge(
      std::string_view setting_name,
      base::RepeatingClosure on_enabled,
      base::RepeatingClosure on_disabled,
      base::RepeatingCallback<bool()> is_subject_to_parental_controls,
      bool initial_value = false);
  FakeContentFiltersObserverBridge(const FakeContentFiltersObserverBridge&) =
      delete;
  FakeContentFiltersObserverBridge& operator=(
      const FakeContentFiltersObserverBridge&) = delete;
  ~FakeContentFiltersObserverBridge() override;

  // Override to suppress initialization of the java bridge.
  void Init() override;
  void Shutdown() override;

  // Set mocked value and trigger native code callbacks.
  void SetEnabled(bool enabled) override;

  base::WeakPtr<FakeContentFiltersObserverBridge> GetWeakPtr();

 private:
  bool initial_value_ = false;
  base::WeakPtrFactory<FakeContentFiltersObserverBridge> weak_ptr_factory_{
      this};
};
#endif  // BUILDFLAG(IS_ANDROID)

// Offers access to the protected constructor of SupervisedUserService, used
// to inject fake content filters observers (with initial values described in
// initial_state)
class TestSupervisedUserService : public SupervisedUserService {
 public:
  // Matching constructor of SupervisedUserService.
  TestSupervisedUserService(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      PrefService& user_prefs,
      SupervisedUserSettingsService& settings_service,
      SupervisedUserContentFiltersService* content_filters_service,
      syncer::SyncService* sync_service,
      std::unique_ptr<SupervisedUserURLFilter> url_filter,
      std::unique_ptr<SupervisedUserService::PlatformDelegate> platform_delegate
#if BUILDFLAG(IS_ANDROID)
      ,
      ContentFiltersObserverBridge::Factory
          content_filters_observer_bridge_factory
#endif
  );

  // Constructor that takes the initial state of supervision.
  TestSupervisedUserService(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      PrefService& user_prefs,
      SupervisedUserSettingsService& settings_service,
      SupervisedUserContentFiltersService* content_filters_service,
      syncer::SyncService* sync_service,
      std::unique_ptr<SupervisedUserURLFilter> url_filter,
      std::unique_ptr<SupervisedUserService::PlatformDelegate>
          platform_delegate,
      InitialSupervisionState initial_state);

#if BUILDFLAG(IS_ANDROID)
  base::WeakPtr<FakeContentFiltersObserverBridge>
  browser_content_filters_observer_weak_ptr();
  base::WeakPtr<FakeContentFiltersObserverBridge>
  search_content_filters_observer_weak_ptr();
#endif  // BUILDFLAG(IS_ANDROID)
};

// Configures a handy set of components that form supervised user features, for
// unit testing. This is a lightweight, unit-test oriented alternative to a
// TestingProfile with enabled supervision.
// Requires single-threaded task environment for unit tests (see
// base::test::TaskEnvironment), and requires that Shutdown() is called.
class SupervisedUserTestEnvironment {
 public:
  explicit SupervisedUserTestEnvironment(
      InitialSupervisionState initial_state =
          InitialSupervisionState::kUnsupervised);
  explicit SupervisedUserTestEnvironment(
      std::unique_ptr<MetricsServiceAccessorDelegateMock>
          metrics_service_accessor_delegate,
      InitialSupervisionState initial_state =
          InitialSupervisionState::kUnsupervised);

  SupervisedUserTestEnvironment(const SupervisedUserTestEnvironment&) = delete;
  SupervisedUserTestEnvironment& operator=(
      const SupervisedUserTestEnvironment&) = delete;
  ~SupervisedUserTestEnvironment();

  SupervisedUserURLFilter* url_filter() const;
  TestSupervisedUserService* service() const;
  PrefService* pref_service();
  sync_preferences::TestingPrefServiceSyncable* pref_service_syncable();
  safe_search_api::FakeURLCheckerClient* url_checker_client();

#if BUILDFLAG(IS_ANDROID)
  base::WeakPtr<FakeContentFiltersObserverBridge>
  browser_content_filters_observer();
  base::WeakPtr<FakeContentFiltersObserverBridge>
  search_content_filters_observer();
#endif  // BUILDFLAG(IS_ANDROID)

  // Simulators of parental controls. Instance methods use services from this
  // test environment, while static methods are suitable for heavier testing
  // profile use.

  // SetWebFilterType methods simulate the custodian modifying "Google Chrome
  // and Web" settings.
  void SetWebFilterType(WebFilterType web_filter_type);
  static void SetWebFilterType(WebFilterType web_filter_type,
                               SupervisedUserSettingsService& service);

  // SetManualFilterForHosts methods simulate the custodian modifying manual
  // hosts overrides.
  void SetManualFilterForHosts(std::map<std::string, bool> exceptions);
  void SetManualFilterForHost(std::string_view host, bool allowlist);
  static void SetManualFilterForHost(std::string_view host,
                                     bool allowlist,
                                     SupervisedUserSettingsService& service);

  // SetManualFilterForUrl methods simulate the custodian modifying manual urls
  // overrides.
  void SetManualFilterForUrl(std::string_view url, bool allowlist);
  static void SetManualFilterForUrl(std::string_view url,
                                    bool allowlist,
                                    SupervisedUserSettingsService& service);

  void Shutdown();

 private:
  SupervisedUserPrefStoreTestEnvironment pref_store_environment_;

  signin::IdentityTestEnvironment identity_test_env_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  syncer::MockSyncService sync_service_;

  // Core services under test
  std::unique_ptr<TestSupervisedUserService> service_;
  std::unique_ptr<SupervisedUserMetricsService> metrics_service_;

  // The objects are actually owned by the service_, but are referenced here for
  // convenience.
  raw_ptr<safe_search_api::FakeURLCheckerClient> url_checker_client_;
};
}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_TEST_ENVIRONMENT_H_
