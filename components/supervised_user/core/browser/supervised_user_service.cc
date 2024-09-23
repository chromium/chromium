// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/supervised_user_service.h"

#include <memory>

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
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/supervised_user/core/browser/kids_chrome_management_url_checker_client.h"
#include "components/supervised_user/core/browser/permission_request_creator_impl.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/supervised_user/core/browser/supervised_user_service_observer.h"
#include "components/supervised_user/core/browser/supervised_user_settings_service.h"
#include "components/supervised_user/core/browser/supervised_user_url_filter.h"
#include "components/supervised_user/core/browser/supervised_user_utils.h"
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
  FirstTimeInterstitialBannerState banner_state =
      static_cast<FirstTimeInterstitialBannerState>(
          user_prefs_->GetInteger(prefs::kFirstTimeInterstitialBannerState));
  banner_state = GetUpdatedBannerState(banner_state);

  user_prefs_->SetInteger(prefs::kFirstTimeInterstitialBannerState,
                          static_cast<int>(banner_state));
  SetActive(supervised_user::IsSubjectToParentalControls(user_prefs_.get()));
}

SupervisedUserURLFilter* SupervisedUserService::GetURLFilter() const {
  return url_filter_.get();
}

void SupervisedUserService::SetURLFilterForTesting(
    std::unique_ptr<SupervisedUserURLFilter> test_filter) {
  url_filter_ = std::move(test_filter);
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

bool SupervisedUserService::HasACustodian() const {
  return !GetCustodianEmailAddress().empty() ||
         !GetSecondCustodianEmailAddress().empty();
}

bool SupervisedUserService::IsBlockedURL(const GURL& url) const {
  // TODO(b/359161670): prevent access to URL filtering through lifecycle events
  // rather than individually checking active state.
  if (!active_) {
    return false;
  }
  return GetURLFilter()->GetFilteringBehaviorForURL(url) ==
         supervised_user::FilteringBehavior::kBlock;
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
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    PrefService& user_prefs,
    SupervisedUserSettingsService& settings_service,
    syncer::SyncService* sync_service,
    std::unique_ptr<SupervisedUserURLFilter::Delegate> url_filter_delegate,
    std::unique_ptr<SupervisedUserService::PlatformDelegate> platform_delegate,
    bool can_show_first_time_interstitial_banner)
    : user_prefs_(user_prefs),
      settings_service_(settings_service),
      sync_service_(sync_service),
      identity_manager_(identity_manager),
      url_loader_factory_(url_loader_factory),
      platform_delegate_(std::move(platform_delegate)),
      can_show_first_time_interstitial_banner_(
          can_show_first_time_interstitial_banner) {
  url_filter_ = std::make_unique<SupervisedUserURLFilter>(
      user_prefs, std::move(url_filter_delegate));
}

FirstTimeInterstitialBannerState SupervisedUserService::GetUpdatedBannerState(
    const FirstTimeInterstitialBannerState original_state) {
  FirstTimeInterstitialBannerState target_state = original_state;
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_IOS)
  if (original_state != FirstTimeInterstitialBannerState::kSetupComplete &&
      can_show_first_time_interstitial_banner_) {
    target_state = FirstTimeInterstitialBannerState::kNeedToShow;
  } else {
    target_state = FirstTimeInterstitialBannerState::kSetupComplete;
  }
#else
  target_state = FirstTimeInterstitialBannerState::kSetupComplete;
#endif
  return target_state;
}

void SupervisedUserService::SetActive(bool active) {
  if (active_ == active) {
    return;
  }
  active_ = active;

  settings_service_->SetActive(active_);

  // Trigger a sync reconfig to enable/disable the right SU data types.
  // The logic to do this lives in the
  // SupervisedUserSettingsDataTypeController.
  // TODO(crbug.com/40620346): Get rid of this hack and instead call
  // DataTypePreconditionChanged from the controller.
  if (sync_service_ &&
      sync_service_->GetUserSettings()->IsInitialSyncFeatureSetupComplete()) {
    // Trigger a reconfig by grabbing a SyncSetupInProgressHandle and
    // immediately releasing it again (via the temporary unique_ptr going away).
    sync_service_->GetSetupInProgressHandle();
  }

  if (active_) {
    // Initialize SafeSites URL checker.
    GetURLFilter()->SetURLCheckerClient(
        std::make_unique<KidsChromeManagementURLCheckerClient>(
            identity_manager_, url_loader_factory_,
            platform_delegate_->GetCountryCode(),
            platform_delegate_->GetChannel()));

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

    remote_web_approvals_manager_.AddApprovalRequestCreator(
        std::make_unique<PermissionRequestCreatorImpl>(identity_manager_,
                                                       url_loader_factory_));

    // Initialize the filter.
    OnDefaultFilteringBehaviorChanged();
    OnSafeSitesSettingChanged();
    UpdateManualHosts();
    UpdateManualURLs();

    GetURLFilter()->SetFilterInitialized(true);
    current_web_filter_type_ = url_filter_->GetWebFilterType();
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

    url_filter_->Clear();
    for (SupervisedUserServiceObserver& observer : observer_list_) {
      observer.OnURLFilterChanged();
    }
  }
}

void SupervisedUserService::OnCustodianInfoChanged() {
  for (SupervisedUserServiceObserver& observer : observer_list_) {
    observer.OnCustodianInfoChanged();
  }
}

void SupervisedUserService::OnSupervisedUserIdChanged() {
  bool is_child =
      supervised_user::IsSubjectToParentalControls(user_prefs_.get());
  if (is_child) {
    // When supervision is enabled, close any incognito windows/tabs that may
    // be open for this profile. These windows cannot be created after the
    // user is signed in, and closing existing ones avoids unexpected
    // behavior due to baked-in assumptions in the SupervisedUser code.
    platform_delegate_->CloseIncognitoTabs();
  }
  SetActive(is_child);
}

void SupervisedUserService::OnDefaultFilteringBehaviorChanged() {
  int behavior_value =
      user_prefs_->GetInteger(prefs::kDefaultSupervisedUserFilteringBehavior);
  supervised_user::FilteringBehavior behavior =
      SupervisedUserURLFilter::BehaviorFromInt(behavior_value);
  url_filter_->SetDefaultFilteringBehavior(behavior);

  for (SupervisedUserServiceObserver& observer : observer_list_) {
    observer.OnURLFilterChanged();
  }

  WebFilterType filter_type = url_filter_->GetWebFilterType();
  if (!AreWebFilterPrefsDefault(*user_prefs_) &&
      current_web_filter_type_ != filter_type) {
    url_filter_->ReportWebFilterTypeMetrics();
    current_web_filter_type_ = filter_type;
  }
}

void SupervisedUserService::OnSafeSitesSettingChanged() {
  WebFilterType filter_type = url_filter_->GetWebFilterType();
  if (!AreWebFilterPrefsDefault(*user_prefs_) &&
      current_web_filter_type_ != filter_type) {
    url_filter_->ReportWebFilterTypeMetrics();
    current_web_filter_type_ = filter_type;
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
  url_filter_->SetManualHosts(std::move(host_map));

  for (SupervisedUserServiceObserver& observer : observer_list_) {
    observer.OnURLFilterChanged();
  }

  if (!AreWebFilterPrefsDefault(*user_prefs_)) {
    url_filter_->ReportManagedSiteListMetrics();
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
  url_filter_->SetManualURLs(std::move(url_map));

  for (SupervisedUserServiceObserver& observer : observer_list_) {
    observer.OnURLFilterChanged();
  }

  if (!AreWebFilterPrefsDefault(*user_prefs_)) {
    url_filter_->ReportManagedSiteListMetrics();
  }
}

void SupervisedUserService::Shutdown() {
  if (!did_init_) {
    return;
  }
  DCHECK(!did_shutdown_);
  did_shutdown_ = true;
  if (supervised_user::IsSubjectToParentalControls(user_prefs_.get())) {
    base::RecordAction(UserMetricsAction("ManagedUsers_QuitBrowser"));
  }
  SetActive(false);
}

void SupervisedUserService::MarkFirstTimeInterstitialBannerShown() const {
  if (ShouldShowFirstTimeInterstitialBanner()) {
    user_prefs_->SetInteger(
        prefs::kFirstTimeInterstitialBannerState,
        static_cast<int>(FirstTimeInterstitialBannerState::kSetupComplete));
  }
}

bool SupervisedUserService::ShouldShowFirstTimeInterstitialBanner() const {
  FirstTimeInterstitialBannerState banner_state =
      static_cast<FirstTimeInterstitialBannerState>(
          user_prefs_->GetInteger(prefs::kFirstTimeInterstitialBannerState));
  return banner_state == FirstTimeInterstitialBannerState::kNeedToShow;
}
}  // namespace supervised_user
