// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/supervised_user_test_environment.h"

#include <memory>
#include <ostream>
#include <string_view>
#include <utility>

#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_notifier_impl.h"
#include "components/prefs/pref_service.h"
#include "components/safe_search_api/fake_url_checker_client.h"
#include "components/supervised_user/core/browser/supervised_user_content_filters_service.h"
#include "components/supervised_user/core/browser/supervised_user_metrics_service.h"
#include "components/supervised_user/core/browser/supervised_user_pref_store.h"
#include "components/supervised_user/core/browser/supervised_user_url_filter.h"
#include "components/supervised_user/core/browser/supervised_user_url_filtering_service.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "components/supervised_user/test_support/supervised_user_url_filter_test_utils.h"
#include "components/sync/base/data_type.h"
#include "components/sync/model/sync_change_processor.h"
#include "components/sync/model/sync_data.h"
#include "components/sync/test/fake_sync_change_processor.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"

namespace supervised_user {

// Defined in supervised_user_constants.h
void PrintTo(const WebFilterType& web_filter_type, ::std::ostream* os) {
  *os << WebFilterTypeToDisplayString(web_filter_type);
}

namespace {
// Just like SupervisedUserPrefStore. The difference is that
// SupervisedUserPrefStore does not offer TestingPrefStore interface, but this
// one does (by actually wrapping SupervisedUserPrefStore).
class SupervisedUserTestingPrefStore : public TestingPrefStore,
                                       public PrefStore::Observer {
 public:
  SupervisedUserTestingPrefStore(
      SupervisedUserSettingsService* settings_service,
      SupervisedUserContentFiltersService* content_filters_service)
      : pref_store_(base::MakeRefCounted<SupervisedUserPrefStore>(
            settings_service,
            content_filters_service)) {
    observation_.Observe(pref_store_.get());
  }

 private:
  ~SupervisedUserTestingPrefStore() override = default;

  void OnPrefValueChanged(std::string_view key) override {
    const base::Value* value = nullptr;
    // Flags are ignored in the TestingPrefStore.
    if (pref_store_->GetValue(key, &value)) {
      SetValue(key, value->Clone(), /*flags=*/0);
    } else {
      RemoveValue(key, /*flags=*/0);
    }
  }

  void OnInitializationCompleted(bool succeeded) override {
    CHECK(succeeded) << "During tests initialization must succeed";
    SetInitializationCompleted();
  }

  scoped_refptr<PrefStore> pref_store_;
  base::ScopedObservation<PrefStore, PrefStore::Observer> observation_{this};
};

void SetManualFilter(std::string_view content_pack_setting,
                     std::string_view entry,
                     bool allowlist,
                     SupervisedUserSettingsService& settings_service) {
  const base::Value::Dict& local_settings =
      settings_service.LocalSettingsForTest();
  base::Value::Dict dict_to_insert;

  if (const base::Value::Dict* dict_value =
          local_settings.FindDict(content_pack_setting)) {
    dict_to_insert = dict_value->Clone();
  }

  dict_to_insert.Set(entry, allowlist);
  settings_service.SetLocalSetting(content_pack_setting,
                                   std::move(dict_to_insert));
}
}  // namespace

SupervisedUserSettingsService* InitializeSettingsServiceForTesting(
    SupervisedUserSettingsService* settings_service) {
  // Note: this pref store is not a part of any pref service, but rather a
  // convenient storage backend of the supervised user settings service.
  scoped_refptr<TestingPrefStore> backing_pref_store =
      base::MakeRefCounted<TestingPrefStore>();
  backing_pref_store->SetInitializationCompleted();

  settings_service->Init(backing_pref_store);
  settings_service->MergeDataAndStartSyncing(
      syncer::SUPERVISED_USER_SETTINGS, syncer::SyncDataList(),
      std::unique_ptr<syncer::SyncChangeProcessor>(
          new syncer::FakeSyncChangeProcessor));

  return settings_service;
}

scoped_refptr<TestingPrefStore> CreateTestingPrefStore(
    SupervisedUserSettingsService* settings_service,
    SupervisedUserContentFiltersService* content_filters_service) {
  return base::MakeRefCounted<SupervisedUserTestingPrefStore>(
      settings_service, content_filters_service);
}

bool SupervisedUserMetricsServiceExtensionDelegateFake::
    RecordExtensionsMetrics() {
  return false;
}

SupervisedUserPrefStoreTestEnvironment::
    SupervisedUserPrefStoreTestEnvironment() {
  RegisterProfilePrefs(syncable_pref_service_->registry());
  SupervisedUserMetricsService::RegisterProfilePrefs(
      syncable_pref_service_->registry());

  // Supervised user infra is not owning these prefs but is using them: enable
  // conditionally only if not already registered (to avoid double
  // registration).
  if (syncable_pref_service_->FindPreference(
          policy::policy_prefs::kIncognitoModeAvailability) == nullptr) {
    syncable_pref_service_->registry()->RegisterIntegerPref(
        policy::policy_prefs::kIncognitoModeAvailability,
        static_cast<int>(policy::IncognitoModeAvailability::kEnabled));
  }
  if (syncable_pref_service_->FindPreference(
          policy::policy_prefs::kForceGoogleSafeSearch) == nullptr) {
    syncable_pref_service_->registry()->RegisterBooleanPref(
        policy::policy_prefs::kForceGoogleSafeSearch, false);
  }
}

SupervisedUserPrefStoreTestEnvironment::
    ~SupervisedUserPrefStoreTestEnvironment() = default;

void SupervisedUserPrefStoreTestEnvironment::ConfigureInitialValues(
    InitialSupervisionState initial_state) {
  // These initial states indicate that the user is supervised, so the main pref
  // must be set.
  switch (initial_state) {
    case InitialSupervisionState::kFamilyLinkDefault:
    case InitialSupervisionState::kFamilyLinkTryToBlockMatureSites:
      EnableParentalControls(*syncable_pref_service_);
      break;
    case InitialSupervisionState::kFamilyLinkAllowAllSites:
      EnableParentalControls(*syncable_pref_service_);
      settings_service_.SetLocalSetting(kSafeSitesEnabled, base::Value(false));
      break;
    case InitialSupervisionState::kFamilyLinkCertainSites:
      EnableParentalControls(*syncable_pref_service_);
      settings_service_.SetLocalSetting(kSafeSitesEnabled, base::Value(false));
      settings_service_.SetLocalSetting(
          supervised_user::kContentPackDefaultFilteringBehavior,
          base::Value(static_cast<int>(FilteringBehavior::kBlock)));
      break;
    default:
      break;
  }
}

void SupervisedUserPrefStoreTestEnvironment::Shutdown() {
  settings_service_.Shutdown();
}

SupervisedUserSettingsService*
SupervisedUserPrefStoreTestEnvironment::settings_service() {
  return &settings_service_;
}

SupervisedUserContentFiltersService*
SupervisedUserPrefStoreTestEnvironment::content_filters_service() {
  return &content_filters_service_;
}

PrefService* SupervisedUserPrefStoreTestEnvironment::pref_service() {
  return syncable_pref_service_.get();
}

SupervisedUserTestEnvironment::SupervisedUserTestEnvironment(
    InitialSupervisionState initial_state)
    : SupervisedUserTestEnvironment(
          std::make_unique<MetricsServiceAccessorDelegateMock>(),
          initial_state) {}

SupervisedUserTestEnvironment::SupervisedUserTestEnvironment(
    std::unique_ptr<MetricsServiceAccessorDelegateMock>
        metrics_service_accessor_delegate,
    InitialSupervisionState initial_state) {
#if BUILDFLAG(IS_ANDROID)
  if (initial_state ==
      InitialSupervisionState::kSupervisedWithAllContentFilters) {
    android_parental_controls_.SetBrowserContentFiltersEnabledForTesting(true);
    android_parental_controls_.SetSearchContentFiltersEnabledForTesting(true);
  }
#endif  // BUILDFLAG(IS_ANDROID)

  std::unique_ptr<safe_search_api::FakeURLCheckerClient> client =
      std::make_unique<safe_search_api::FakeURLCheckerClient>();
  url_checker_client_ = client.get();
  pref_store_environment_.ConfigureInitialValues(initial_state);
  service_ = std::make_unique<SupervisedUserService>(
      identity_test_env_.identity_manager(),
      test_url_loader_factory_.GetSafeWeakWrapper(),
      *pref_store_environment_.pref_service(),
      *pref_store_environment_.settings_service(),
      pref_store_environment_.content_filters_service(), &sync_service_,
      std::make_unique<SupervisedUserURLFilter>(
          *pref_store_environment_.pref_service(),
          std::make_unique<FakeURLFilterDelegate>(), std::move(client)),
      std::make_unique<FakePlatformDelegate>()
#if BUILDFLAG(IS_ANDROID)
          ,
      android_parental_controls_
#endif  // BUILDFLAG(IS_ANDROID)
  );

  url_filtering_service_ = std::make_unique<SupervisedUserUrlFilteringService>(
      *service_.get(), *pref_store_environment_.settings_service());
  metrics_service_ = std::make_unique<SupervisedUserMetricsService>(
      pref_store_environment_.pref_service(), *service_.get(),
      *url_filtering_service_.get(),
      std::make_unique<SupervisedUserMetricsServiceExtensionDelegateFake>(),
      std::move(metrics_service_accessor_delegate));
}

SupervisedUserTestEnvironment::~SupervisedUserTestEnvironment() = default;
void SupervisedUserTestEnvironment::Shutdown() {
  metrics_service_->Shutdown();
  service_->Shutdown();
  pref_store_environment_.Shutdown();
}

void SupervisedUserTestEnvironment::SetWebFilterType(
    WebFilterType web_filter_type) {
  SetWebFilterType(web_filter_type,
                   *pref_store_environment_.settings_service());
}
void SupervisedUserTestEnvironment::SetWebFilterType(
    WebFilterType web_filter_type,
    SupervisedUserSettingsService& settings_service) {
  switch (web_filter_type) {
    case WebFilterType::kAllowAllSites:
      settings_service.SetLocalSetting(
          kContentPackDefaultFilteringBehavior,
          base::Value(static_cast<int>(FilteringBehavior::kAllow)));
      settings_service.SetLocalSetting(kSafeSitesEnabled, base::Value(false));
      break;
    case WebFilterType::kTryToBlockMatureSites:
      settings_service.SetLocalSetting(
          kContentPackDefaultFilteringBehavior,
          base::Value(static_cast<int>(FilteringBehavior::kAllow)));
      settings_service.SetLocalSetting(kSafeSitesEnabled, base::Value(true));
      break;
    case WebFilterType::kCertainSites:
      settings_service.SetLocalSetting(
          kContentPackDefaultFilteringBehavior,
          base::Value(static_cast<int>(FilteringBehavior::kBlock)));

      // Value of kSupervisedUserSafeSites is not important here.
      break;
    case WebFilterType::kDisabled:
      NOTREACHED() << "To disable the URL filter, use "
                      "supervised_user::DisableParentalControls(.)";
    case WebFilterType::kMixed:
      NOTREACHED() << "That value is not intended to be set, but is rather "
                      "used to indicate multiple settings used in profiles "
                      "in metrics.";
  }
}

void SupervisedUserTestEnvironment::SetManualFilterForHosts(
    std::map<std::string, bool> exceptions) {
  for (const auto& [host, allowlist] : exceptions) {
    SetManualFilterForHost(host, allowlist);
  }
}

void SupervisedUserTestEnvironment::SetManualFilterForHost(
    std::string_view host,
    bool allowlist) {
  SetManualFilterForHost(host, allowlist,
                         *pref_store_environment_.settings_service());
}
void SupervisedUserTestEnvironment::SetManualFilterForHost(
    std::string_view host,
    bool allowlist,
    SupervisedUserSettingsService& service) {
  SetManualFilter(kContentPackManualBehaviorHosts, host, allowlist, service);
}

void SupervisedUserTestEnvironment::SetManualFilterForUrl(std::string_view url,
                                                          bool allowlist) {
  SetManualFilterForUrl(url, allowlist,
                        *pref_store_environment_.settings_service());
}
void SupervisedUserTestEnvironment::SetManualFilterForUrl(
    std::string_view url,
    bool allowlist,
    SupervisedUserSettingsService& service) {
  SetManualFilter(kContentPackManualBehaviorURLs, url, allowlist, service);
}

SupervisedUserURLFilter* SupervisedUserTestEnvironment::url_filter() const {
  return service()->GetURLFilter();
}
SupervisedUserService* SupervisedUserTestEnvironment::service() const {
  return service_.get();
}
SupervisedUserUrlFilteringService*
SupervisedUserTestEnvironment::url_filtering_service() const {
  return url_filtering_service_.get();
}
PrefService* SupervisedUserTestEnvironment::pref_service() {
  return pref_store_environment_.pref_service();
}
sync_preferences::TestingPrefServiceSyncable*
SupervisedUserTestEnvironment::pref_service_syncable() {
  return static_cast<sync_preferences::TestingPrefServiceSyncable*>(
      pref_service());
}
safe_search_api::FakeURLCheckerClient*
SupervisedUserTestEnvironment::url_checker_client() {
  return url_checker_client_.get();
}
#if BUILDFLAG(IS_ANDROID)
AndroidParentalControls*
SupervisedUserTestEnvironment::android_parental_controls() {
  return &android_parental_controls_;
}
#endif  // BUILDFLAG(IS_ANDROID)

MetricsServiceAccessorDelegateMock::MetricsServiceAccessorDelegateMock() =
    default;
MetricsServiceAccessorDelegateMock::~MetricsServiceAccessorDelegateMock() =
    default;
}  // namespace supervised_user
