// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_TEST_ENVIRONMENT_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_TEST_ENVIRONMENT_H_

#include <map>
#include <memory>
#include <string_view>

#include "base/memory/ref_counted.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_store.h"
#include "components/safe_search_api/fake_url_checker_client.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
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

// Launches the service from empty settings, typically during context
// initialization.
SupervisedUserSettingsService* InitializeSettingsServiceForTesting(
    SupervisedUserSettingsService* settings_service);

// Prepares a pref service component for use in test.
scoped_refptr<TestingPrefStore> CreateTestingPrefStore(
    SupervisedUserSettingsService* settings_service);

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

  void Shutdown();

 private:
  SupervisedUserSettingsService settings_service_;
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable>
      syncable_pref_service_ =
          std::make_unique<sync_preferences::TestingPrefServiceSyncable>(
              /*managed_prefs=*/base::MakeRefCounted<TestingPrefStore>(),
              /*supervised_user_prefs=*/
              CreateTestingPrefStore(
                  InitializeSettingsServiceForTesting(&settings_service_)),
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

#if BUILDFLAG(IS_ANDROID)
// Fake implementation of ContentFiltersObserverBridge for testing. Imitates
// events that would normally be produced by the Android's secure settings
// (which store content filter settings). Content bridge is initialized with
// "disabled" setting.
class FakeContentFiltersObserverBridge final
    : public ContentFiltersObserverBridge {
 public:
  FakeContentFiltersObserverBridge(std::string_view setting_name,
                                   base::RepeatingClosure on_enabled,
                                   base::RepeatingClosure on_disabled);
  FakeContentFiltersObserverBridge(const FakeContentFiltersObserverBridge&) =
      delete;
  FakeContentFiltersObserverBridge& operator=(
      const FakeContentFiltersObserverBridge&) = delete;
  ~FakeContentFiltersObserverBridge() override;

  // Override to suppress initialization of the java bridge.
  void Init() override;
  void Shutdown() override;

  // Override to return mocked value.
  bool IsEnabled() const override;
  // Set mocked value.
  void SetEnabled(bool enabled);

 private:
  // Stores actual value (in place of secure settings storage). Note: In prod
  // environment there is one setting value for all profiles, but this fake
  // is bound to a specific profile.
  bool enabled_ = false;
};
#endif  // BUILDFLAG(IS_ANDROID)

// Configures a handy set of components that form supervised user features, for
// unit testing. This is a lightweight, unit-test oriented alternative to a
// TestingProfile with enabled supervision.
// Requires single-threaded task environment for unit tests (see
// base::test::TaskEnvironment), and requires that Shutdown() is called.
class SupervisedUserTestEnvironment {
 public:
  SupervisedUserTestEnvironment();
  SupervisedUserTestEnvironment(const SupervisedUserTestEnvironment&) = delete;
  SupervisedUserTestEnvironment& operator=(
      const SupervisedUserTestEnvironment&) = delete;
  ~SupervisedUserTestEnvironment();

  SupervisedUserURLFilter* url_filter() const;
  SupervisedUserService* service() const;
  PrefService* pref_service();
  sync_preferences::TestingPrefServiceSyncable* pref_service_syncable();
  safe_search_api::FakeURLCheckerClient* url_checker_client();
#if BUILDFLAG(IS_ANDROID)
  FakeContentFiltersObserverBridge* browser_content_filters_observer();
  FakeContentFiltersObserverBridge* search_content_filters_observer();
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
#if BUILDFLAG(IS_ANDROID)
  // Two fake content filters observers are created for testing purposes. One
  // for the browser content filters and one for the search content filters.
  std::unique_ptr<ContentFiltersObserverBridge> CreateBridge(
      std::string_view setting_name,
      base::RepeatingClosure on_enabled,
      base::RepeatingClosure on_disabled);
#endif  // BUILDFLAG(IS_ANDROID)
  SupervisedUserPrefStoreTestEnvironment pref_store_environment_;

  signin::IdentityTestEnvironment identity_test_env_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  syncer::MockSyncService sync_service_;

  // Core services under test
  std::unique_ptr<SupervisedUserService> service_;
  std::unique_ptr<SupervisedUserMetricsService> metrics_service_;

  // The objects are actually owned by the service_, but are referenced here for
  // convenience.
  raw_ptr<safe_search_api::FakeURLCheckerClient> url_checker_client_;
#if BUILDFLAG(IS_ANDROID)
  raw_ptr<FakeContentFiltersObserverBridge> browser_content_filters_observer_;
  raw_ptr<FakeContentFiltersObserverBridge> search_content_filters_observer_;
#endif  // BUILDFLAG(IS_ANDROID)
};
}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_TEST_ENVIRONMENT_H_
