// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/supervised_user_service.h"

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "base/version.h"
#include "build/build_config.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/supervised_user/core/browser/kids_chrome_management_client.h"
#include "components/supervised_user/core/browser/supervised_user_service_observer.h"
#include "components/supervised_user/core/browser/supervised_user_settings_service.h"
#include "components/supervised_user/core/browser/supervised_user_url_filter.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "ui/base/l10n/l10n_util.h"

using base::UserMetricsAction;

namespace supervised_user {

SupervisedUserService::~SupervisedUserService() {
  DCHECK(!did_init_ || did_shutdown_);
  url_filter_.RemoveObserver(this);
}

// static
void SupervisedUserService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(prefs::kSupervisedUserManualHosts);
  registry->RegisterDictionaryPref(prefs::kSupervisedUserManualURLs);
  registry->RegisterIntegerPref(prefs::kDefaultSupervisedUserFilteringBehavior,
                                SupervisedUserURLFilter::ALLOW);
  registry->RegisterBooleanPref(prefs::kSupervisedUserSafeSites, true);
  for (const char* pref : kCustodianInfoPrefs) {
    registry->RegisterStringPref(pref, std::string());
  }
  registry->RegisterIntegerPref(
      prefs::kFirstTimeInterstitialBannerState,
      static_cast<int>(
          supervised_user::FirstTimeInterstitialBannerState::kUnknown));
}

void SupervisedUserService::Init() {
  DCHECK(!did_init_);
  did_init_ = true;
  DCHECK(settings_service_->IsReady());

  pref_change_registrar_.Init(&user_prefs_.get());
  pref_change_registrar_.Add(
      prefs::kSupervisedUserId,
      base::BindRepeating(&SupervisedUserService::OnSupervisedUserIdChanged,
                          base::Unretained(this)));
  supervised_user::FirstTimeInterstitialBannerState banner_state =
      static_cast<supervised_user::FirstTimeInterstitialBannerState>(
          user_prefs_->GetInteger(prefs::kFirstTimeInterstitialBannerState));
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  if (supervised_user::CanDisplayFirstTimeInterstitialBanner()) {
    if (banner_state ==
            supervised_user::FirstTimeInterstitialBannerState::kUnknown &&
        can_show_first_time_interstitial_banner_) {
      banner_state =
          supervised_user::FirstTimeInterstitialBannerState::kNeedToShow;
    } else {
      banner_state =
          supervised_user::FirstTimeInterstitialBannerState::kSetupComplete;
    }
  }
#else
  banner_state =
      supervised_user::FirstTimeInterstitialBannerState::kSetupComplete;
#endif

  user_prefs_->SetInteger(prefs::kFirstTimeInterstitialBannerState,
                          static_cast<int>(banner_state));
  SetActive(IsSubjectToParentalControls());
}

void SupervisedUserService::SetDelegate(Delegate* delegate) {
  if (delegate) {
    // Changing delegates isn't allowed.
    DCHECK(!delegate_);
  } else {
    // If the delegate is removed, deactivate first to give the old delegate a
    // chance to clean up.
    SetActive(false);
  }
  delegate_ = delegate;
}

SupervisedUserURLFilter* SupervisedUserService::GetURLFilter() {
  return &url_filter_;
}

// static
std::string SupervisedUserService::GetExtensionRequestId(
    const std::string& extension_id,
    const base::Version& version) {
  return base::StringPrintf("%s:%s", extension_id.c_str(),
                            version.GetString().c_str());
}

std::string SupervisedUserService::GetCustodianEmailAddress() const {
  return user_prefs_->GetString(prefs::kSupervisedUserCustodianEmail);
}

std::string SupervisedUserService::GetCustodianObfuscatedGaiaId() const {
  return user_prefs_->GetString(
      prefs::kSupervisedUserCustodianObfuscatedGaiaId);
}

std::string SupervisedUserService::GetCustodianName() const {
  std::string name =
      user_prefs_->GetString(prefs::kSupervisedUserCustodianName);
  return name.empty() ? GetCustodianEmailAddress() : name;
}

std::string SupervisedUserService::GetSecondCustodianEmailAddress() const {
  return user_prefs_->GetString(prefs::kSupervisedUserSecondCustodianEmail);
}

std::string SupervisedUserService::GetSecondCustodianObfuscatedGaiaId() const {
  return user_prefs_->GetString(
      prefs::kSupervisedUserSecondCustodianObfuscatedGaiaId);
}

std::string SupervisedUserService::GetSecondCustodianName() const {
  std::string name =
      user_prefs_->GetString(prefs::kSupervisedUserSecondCustodianName);
  return name.empty() ? GetSecondCustodianEmailAddress() : name;
}

bool SupervisedUserService::IsURLFilteringEnabled() const {
// TODO(b/271413641): Use capabilities to verify if filtering is enabled on iOS.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
  return IsSubjectToParentalControls();
#else
  return IsSubjectToParentalControls() &&
         base::FeatureList::IsEnabled(
             kFilterWebsitesForSupervisedUsersOnDesktopAndIOS);
#endif
}

bool SupervisedUserService::AreExtensionsPermissionsEnabled() const {
#if BUILDFLAG(ENABLE_EXTENSIONS)
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
  return IsSubjectToParentalControls();
#else
  return IsSubjectToParentalControls() &&
         base::FeatureList::IsEnabled(
             kEnableExtensionsPermissionsForSupervisedUsersOnDesktop);
#endif
#else
  return false;
#endif
}

bool SupervisedUserService::HasACustodian() const {
  return !GetCustodianEmailAddress().empty() ||
         !GetSecondCustodianEmailAddress().empty();
}

void SupervisedUserService::AddObserver(
    SupervisedUserServiceObserver* observer) {
  observer_list_.AddObserver(observer);
}

void SupervisedUserService::RemoveObserver(
    SupervisedUserServiceObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

SupervisedUserService::SupervisedUserService(
    KidsChromeManagementClient* kids_chrome_management_client,
    PrefService& user_prefs,
    SupervisedUserSettingsService& settings_service,
    syncer::SyncService& sync_service,
    ValidateURLSupportCallback check_webstore_url_callback,
    std::unique_ptr<SupervisedUserURLFilter::Delegate> url_filter_delegate,
    bool can_show_first_time_interstitial_banner)
    : user_prefs_(user_prefs),
      settings_service_(settings_service),
      sync_service_(sync_service),
      kids_chrome_management_client_(kids_chrome_management_client),
      delegate_(nullptr),
      url_filter_(std::move(check_webstore_url_callback),
                  std::move(url_filter_delegate)),
      can_show_first_time_interstitial_banner_(
          can_show_first_time_interstitial_banner) {
  url_filter_.AddObserver(this);
}

void SupervisedUserService::ReportNonDefaultWebFilterValue() const {
  if (AreWebFilterPrefsDefault(*user_prefs_)) {
    return;
  }

  url_filter_.ReportManagedSiteListMetrics();
  url_filter_.ReportWebFilterTypeMetrics();
}

void SupervisedUserService::SetActive(bool active) {
  if (active_ == active) {
    return;
  }
  active_ = active;

  if (delegate_) {
    delegate_->SetActive(active_);
  }

  settings_service_->SetActive(active_);

  // Trigger a sync reconfig to enable/disable the right SU data types.
  // The logic to do this lives in the SupervisedUserSyncModelTypeController.
  // TODO(crbug.com/946473): Get rid of this hack and instead call
  // DataTypePreconditionChanged from the controller.
  if (sync_service_->GetUserSettings()->IsInitialSyncFeatureSetupComplete()) {
    // Trigger a reconfig by grabbing a SyncSetupInProgressHandle and
    // immediately releasing it again (via the temporary unique_ptr going away).
    sync_service_->GetSetupInProgressHandle();
  }

  if (active_) {
    pref_change_registrar_.Add(
        prefs::kDefaultSupervisedUserFilteringBehavior,
        base::BindRepeating(
            &SupervisedUserService::OnDefaultFilteringBehaviorChanged,
            base::Unretained(this)));
    pref_change_registrar_.Add(
        prefs::kSupervisedUserSafeSites,
        base::BindRepeating(&SupervisedUserService::OnSafeSitesSettingChanged,
                            base::Unretained(this)));
    pref_change_registrar_.Add(
        prefs::kSupervisedUserManualHosts,
        base::BindRepeating(&SupervisedUserService::UpdateManualHosts,
                            base::Unretained(this)));
    pref_change_registrar_.Add(
        prefs::kSupervisedUserManualURLs,
        base::BindRepeating(&SupervisedUserService::UpdateManualURLs,
                            base::Unretained(this)));
    for (const char* pref : kCustodianInfoPrefs) {
      pref_change_registrar_.Add(
          pref,
          base::BindRepeating(&SupervisedUserService::OnCustodianInfoChanged,
                              base::Unretained(this)));
    }

    // Initialize the filter.
    OnDefaultFilteringBehaviorChanged();
    OnSafeSitesSettingChanged();
    UpdateManualHosts();
    UpdateManualURLs();

    GetURLFilter()->SetFilterInitialized(true);
    current_web_filter_type_ = url_filter_.GetWebFilterType();
  } else {
    remote_web_approvals_manager_.ClearApprovalRequestsCreators();

    pref_change_registrar_.Remove(
        prefs::kDefaultSupervisedUserFilteringBehavior);
    pref_change_registrar_.Remove(prefs::kSupervisedUserSafeSites);
    pref_change_registrar_.Remove(prefs::kSupervisedUserManualHosts);
    pref_change_registrar_.Remove(prefs::kSupervisedUserManualURLs);
    for (const char* pref : kCustodianInfoPrefs) {
      pref_change_registrar_.Remove(pref);
    }

    url_filter_.Clear();
    for (SupervisedUserServiceObserver& observer : observer_list_) {
      observer.OnURLFilterChanged();
    }
  }
}

bool SupervisedUserService::IsSubjectToParentalControls() const {
  return user_prefs_->GetString(prefs::kSupervisedUserId) == kChildAccountSUID;
}

void SupervisedUserService::OnCustodianInfoChanged() {
  for (SupervisedUserServiceObserver& observer : observer_list_) {
    observer.OnCustodianInfoChanged();
  }
}

void SupervisedUserService::OnSupervisedUserIdChanged() {
  SetActive(IsSubjectToParentalControls());
}

void SupervisedUserService::OnDefaultFilteringBehaviorChanged() {
  int behavior_value =
      user_prefs_->GetInteger(prefs::kDefaultSupervisedUserFilteringBehavior);
  SupervisedUserURLFilter::FilteringBehavior behavior =
      SupervisedUserURLFilter::BehaviorFromInt(behavior_value);
  url_filter_.SetDefaultFilteringBehavior(behavior);
  UpdateAsyncUrlChecker();

  for (SupervisedUserServiceObserver& observer : observer_list_) {
    observer.OnURLFilterChanged();
  }

  SupervisedUserURLFilter::WebFilterType filter_type =
      url_filter_.GetWebFilterType();
  if (!AreWebFilterPrefsDefault(*user_prefs_) &&
      current_web_filter_type_ != filter_type) {
    url_filter_.ReportWebFilterTypeMetrics();
    current_web_filter_type_ = filter_type;
  }
}

bool SupervisedUserService::IsSafeSitesEnabled() const {
  return IsSubjectToParentalControls() &&
         user_prefs_->GetBoolean(prefs::kSupervisedUserSafeSites);
}

void SupervisedUserService::OnSafeSitesSettingChanged() {
  UpdateAsyncUrlChecker();

  SupervisedUserURLFilter::WebFilterType filter_type =
      url_filter_.GetWebFilterType();
  if (!AreWebFilterPrefsDefault(*user_prefs_) &&
      current_web_filter_type_ != filter_type) {
    url_filter_.ReportWebFilterTypeMetrics();
    current_web_filter_type_ = filter_type;
  }
}

void SupervisedUserService::UpdateAsyncUrlChecker() {
  int behavior_value =
      user_prefs_->GetInteger(prefs::kDefaultSupervisedUserFilteringBehavior);
  SupervisedUserURLFilter::FilteringBehavior behavior =
      SupervisedUserURLFilter::BehaviorFromInt(behavior_value);

  bool use_online_check =
      IsSafeSitesEnabled() ||
      behavior == SupervisedUserURLFilter::FilteringBehavior::BLOCK;

  if (use_online_check != url_filter_.HasAsyncURLChecker()) {
    if (use_online_check) {
      url_filter_.InitAsyncURLChecker(kids_chrome_management_client_);
    } else {
      url_filter_.ClearAsyncURLChecker();
    }
  }
}

void SupervisedUserService::UpdateManualHosts() {
  const base::Value::Dict& dict =
      user_prefs_->GetDict(prefs::kSupervisedUserManualHosts);
  std::map<std::string, bool> host_map;
  for (auto it : dict) {
    DCHECK(it.second.is_bool());
    host_map[it.first] = it.second.GetIfBool().value_or(false);
  }
  url_filter_.SetManualHosts(std::move(host_map));

  for (SupervisedUserServiceObserver& observer : observer_list_) {
    observer.OnURLFilterChanged();
  }

  if (!AreWebFilterPrefsDefault(*user_prefs_)) {
    url_filter_.ReportManagedSiteListMetrics();
  }
}

void SupervisedUserService::UpdateManualURLs() {
  const base::Value::Dict& dict =
      user_prefs_->GetDict(prefs::kSupervisedUserManualURLs);
  std::map<GURL, bool> url_map;
  for (auto it : dict) {
    DCHECK(it.second.is_bool());
    url_map[GURL(it.first)] = it.second.GetIfBool().value_or(false);
  }
  url_filter_.SetManualURLs(std::move(url_map));

  for (SupervisedUserServiceObserver& observer : observer_list_) {
    observer.OnURLFilterChanged();
  }

  if (!AreWebFilterPrefsDefault(*user_prefs_)) {
    url_filter_.ReportManagedSiteListMetrics();
  }
}

void SupervisedUserService::Shutdown() {
  if (!did_init_) {
    return;
  }
  DCHECK(!did_shutdown_);
  did_shutdown_ = true;
  if (IsSubjectToParentalControls()) {
    base::RecordAction(UserMetricsAction("ManagedUsers_QuitBrowser"));
  }
  SetActive(false);
}

void SupervisedUserService::OnSiteListUpdated() {
  for (SupervisedUserServiceObserver& observer : observer_list_) {
    observer.OnURLFilterChanged();
  }
}

void SupervisedUserService::MarkFirstTimeInterstitialBannerShown() const {
  if (ShouldShowFirstTimeInterstitialBanner()) {
    user_prefs_->SetInteger(
        prefs::kFirstTimeInterstitialBannerState,
        static_cast<int>(
            supervised_user::FirstTimeInterstitialBannerState::kSetupComplete));
  }
}

bool SupervisedUserService::ShouldShowFirstTimeInterstitialBanner() const {
  supervised_user::FirstTimeInterstitialBannerState banner_state =
      static_cast<supervised_user::FirstTimeInterstitialBannerState>(
          user_prefs_->GetInteger(prefs::kFirstTimeInterstitialBannerState));
  return banner_state ==
         supervised_user::FirstTimeInterstitialBannerState::kNeedToShow;
}
}  // namespace supervised_user
