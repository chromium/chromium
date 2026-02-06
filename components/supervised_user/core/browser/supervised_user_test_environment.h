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
#include "components/supervised_user/core/browser/device_parental_controls_noop_impl.h"
#include "components/supervised_user/core/browser/device_parental_controls_url_filter.h"
#include "components/supervised_user/core/browser/family_link_settings_service.h"
#include "components/supervised_user/core/browser/supervised_user_metrics_service.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "components/supervised_user/core/browser/supervised_user_synthetic_field_trial_service_delegate.h"
#include "components/supervised_user/core/browser/supervised_user_url_filtering_service.h"
#include "components/supervised_user/test_support/supervised_user_url_filter_test_utils.h"
#include "components/sync/test/mock_sync_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "services/network/test/test_url_loader_factory.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/supervised_user/core/browser/android/android_parental_controls.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace supervised_user {

using DeviceParentalControlsTestImpl =
#if BUILDFLAG(IS_ANDROID)
    AndroidParentalControls;
#else
    DeviceParentalControlsNoOpImpl;
#endif

// Handy set of initial states of supervision stack, to preset before testing.
enum class InitialSupervisionState : int {
  // Default mode, no family link nor local device supervision.
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
FamilyLinkSettingsService* InitializeSettingsServiceForTesting(
    FamilyLinkSettingsService* settings_service);

// Prepares a pref service component for use in test.
scoped_refptr<TestingPrefStore> CreateTestingPrefStore(
    FamilyLinkSettingsService* settings_service,
    DeviceParentalControls& device_parental_controls);

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
  FamilyLinkSettingsService* settings_service();
  // That's a simplification: in prod environment the parental controls are
  // global, but in the test environment they are per pref service.
  DeviceParentalControlsTestImpl& device_parental_controls();

  void Shutdown();

  // Sets initial values in components like pref service and content filters
  // before services are created.
  void ConfigureInitialValues(InitialSupervisionState initial_state);

 private:
  FamilyLinkSettingsService settings_service_;
  DeviceParentalControlsTestImpl device_parental_controls_;

  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable>
      syncable_pref_service_ =
          std::make_unique<sync_preferences::TestingPrefServiceSyncable>(
              /*managed_prefs=*/base::MakeRefCounted<TestingPrefStore>(),
              /*supervised_user_prefs=*/
              CreateTestingPrefStore(
                  InitializeSettingsServiceForTesting(&settings_service_),
                  device_parental_controls_),
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

class SynteticFieldTrialDelegateMock : public SynteticFieldTrialDelegate {
 public:
  SynteticFieldTrialDelegateMock();
  ~SynteticFieldTrialDelegateMock() override;
  MOCK_METHOD(void,
              RegisterSyntheticFieldTrial,
              (std::string_view trial_name, std::string_view group_name),
              (override));

 private:
  base::WeakPtrFactory<SynteticFieldTrialDelegateMock> weak_ptr_factory_{this};
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
      std::unique_ptr<SynteticFieldTrialDelegateMock>
          synthetic_field_trial_delegate,
      InitialSupervisionState initial_state =
          InitialSupervisionState::kUnsupervised);

  SupervisedUserTestEnvironment(const SupervisedUserTestEnvironment&) = delete;
  SupervisedUserTestEnvironment& operator=(
      const SupervisedUserTestEnvironment&) = delete;
  ~SupervisedUserTestEnvironment();

  MockUrlCheckerClient& family_link_url_checker_client();
  MockUrlCheckerClient& device_parental_controls_url_checker_client();

  FamilyLinkUrlFilter* family_link_url_filter() const;

  SupervisedUserService* service() const;
  SupervisedUserUrlFilteringService* url_filtering_service() const;
  PrefService* pref_service();
  sync_preferences::TestingPrefServiceSyncable* pref_service_syncable();
  DeviceParentalControlsTestImpl& device_parental_controls();

  // Simulators of parental controls. Instance methods use services from this
  // test environment, while static methods are suitable for heavier testing
  // profile use.

  // SetWebFilterType methods simulate the custodian modifying "Google Chrome
  // and Web" settings.
  void SetWebFilterType(WebFilterType web_filter_type);
  static void SetWebFilterType(WebFilterType web_filter_type,
                               FamilyLinkSettingsService& service);

  // SetManualFilterForHosts methods simulate the custodian modifying manual
  // hosts overrides.
  void SetManualFilterForHosts(std::map<std::string, bool> exceptions);
  void SetManualFilterForHost(std::string_view host, bool allowlist);
  static void SetManualFilterForHost(std::string_view host,
                                     bool allowlist,
                                     FamilyLinkSettingsService& service);

  // SetManualFilterForUrl methods simulate the custodian modifying manual urls
  // overrides.
  void SetManualFilterForUrl(std::string_view url, bool allowlist);
  static void SetManualFilterForUrl(std::string_view url,
                                    bool allowlist,
                                    FamilyLinkSettingsService& service);

  void Shutdown();

 private:
  MockUrlCheckerClient family_link_url_checker_client_;
  MockUrlCheckerClient device_parental_controls_url_checker_client_;

  SupervisedUserPrefStoreTestEnvironment pref_store_environment_;

  signin::IdentityTestEnvironment identity_test_env_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  syncer::MockSyncService sync_service_;

  // Core services under test
  std::unique_ptr<SupervisedUserService> service_;
  std::unique_ptr<SupervisedUserUrlFilteringService> url_filtering_service_;
  std::unique_ptr<SupervisedUserMetricsService> metrics_service_;
};
}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_TEST_ENVIRONMENT_H_
