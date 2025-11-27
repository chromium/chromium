// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/supervised_user_service.h"

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "base/version.h"
#include "build/build_config.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/supervised_user/core/browser/kids_chrome_management_url_checker_client.h"
#include "components/supervised_user/core/browser/permission_request_creator_impl.h"
#include "components/supervised_user/core/browser/supervised_user_content_filters_service.h"
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
#include "google_apis/gaia/gaia_id.h"
#include "ui/base/l10n/l10n_util.h"

namespace supervised_user {

namespace {

#if BUILDFLAG(IS_ANDROID)
const char kSupervisionConflictHistogramName[] =
    "SupervisedUsers.FamilyLinkSupervisionConflict";
enum class SupervisionHasConflict : int {
  kNoConflict = 0,
  kHasConflict = 1,
  kMaxValue = kHasConflict,
};
#endif  // BUILDFLAG(IS_ANDROID)

using base::UserMetricsAction;

// All prefs that configure the url filter.
std::array<const char*, 4> kUrlFilterSettingsPrefs = {
    prefs::kDefaultSupervisedUserFilteringBehavior,
    prefs::kSupervisedUserSafeSites, prefs::kSupervisedUserManualHosts,
    prefs::kSupervisedUserManualURLs};

// Helper that extracts custodian data from given preferences.
std::optional<Custodian> GetCustodianFromPrefs(
    const PrefService& user_prefs,
    std::string_view email_address_pref,
    std::string_view name_pref,
    std::string_view gaia_id_pref,
    std::string_view profile_image_url_pref) {
  std::string email(user_prefs.GetString(email_address_pref));
  std::string name(user_prefs.GetString(name_pref));
  GaiaId gaia_id(user_prefs.GetString(gaia_id_pref));
  std::string profile_image_url(user_prefs.GetString(profile_image_url_pref));

  if (email.empty() && name.empty() && gaia_id.empty() &&
      profile_image_url.empty()) {
    return std::nullopt;
  }
  return Custodian((name.empty() ? email : name), email, gaia_id,
                   profile_image_url);
}

// Sentinel that guards against accidental pref changes.
void PrefChangeNotAllowed(const std::string& pref_name) {
  NOTREACHED(base::NotFatalUntil::M150)
      << "Preference change (" << pref_name << ") not allowed.";
}
}  // namespace

Custodian::Custodian(std::string_view name,
                     std::string_view email_address,
                     GaiaId obfuscated_gaia_id,
                     std::string_view profile_image_url)
    : name_(name),
      email_address_(email_address),
      obfuscated_gaia_id_(obfuscated_gaia_id),
      profile_image_url_(profile_image_url) {}
Custodian::Custodian(std::string_view name,
                     std::string_view email_address,
                     std::string_view profile_image_url)
    : Custodian(name, email_address, GaiaId(), profile_image_url) {}

Custodian::Custodian(const Custodian& other) = default;
Custodian::~Custodian() = default;

SupervisedUserService::~SupervisedUserService() {
  DCHECK(did_shutdown_);
}

SupervisedUserURLFilter* SupervisedUserService::GetURLFilter() const {
  return url_filter_.get();
}

bool SupervisedUserService::IsSupervisedLocally() const {
#if BUILDFLAG(IS_ANDROID)
  return IsLocalBrowserFilteringEnabled() || IsLocalSearchFilteringEnabled();
#else
  return false;
#endif
}

bool SupervisedUserService::IsLocalBrowserFilteringEnabled() const {
#if BUILDFLAG(IS_ANDROID)
  return browser_content_filters_observer_->IsEnabled();
#else
  return false;
#endif
}

bool SupervisedUserService::IsLocalSearchFilteringEnabled() const {
#if BUILDFLAG(IS_ANDROID)
  return search_content_filters_observer_->IsEnabled();
#else
  return false;
#endif
}

std::optional<Custodian> SupervisedUserService::GetCustodian() const {
  return GetCustodianFromPrefs(user_prefs_.get(),
                               prefs::kSupervisedUserCustodianEmail,
                               prefs::kSupervisedUserCustodianName,
                               prefs::kSupervisedUserCustodianObfuscatedGaiaId,
                               prefs::kSupervisedUserCustodianProfileImageURL);
}

std::optional<Custodian> SupervisedUserService::GetSecondCustodian() const {
  return GetCustodianFromPrefs(
      user_prefs_.get(), prefs::kSupervisedUserSecondCustodianEmail,
      prefs::kSupervisedUserSecondCustodianName,
      prefs::kSupervisedUserSecondCustodianObfuscatedGaiaId,
      prefs::kSupervisedUserSecondCustodianProfileImageURL);
}

bool SupervisedUserService::IsBlockedURL(const GURL& url) const {
  // TODO(b/359161670): prevent access to URL filtering through lifecycle events
  // rather than individually checking active state.
  if (!IsSubjectToParentalControls(user_prefs_.get())) {
    return false;
  }
  return GetURLFilter()->GetFilteringBehavior(url).IsBlocked();
}

void SupervisedUserService::AddObserver(
    SupervisedUserServiceObserver* observer) {
  observer_list_.AddObserver(observer);
}

void SupervisedUserService::RemoveObserver(
    SupervisedUserServiceObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

// Note: unretained is safe, because the utility that binds callbacks is owned
// by this instance.
SupervisedUserService::SupervisedUserService(
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
    )
    : user_prefs_(user_prefs),
      settings_service_(settings_service),
      content_filters_service_(content_filters_service),
      sync_service_(sync_service),
      identity_manager_(identity_manager),
      url_loader_factory_(url_loader_factory),
      url_filter_(std::move(url_filter)),
      platform_delegate_(std::move(platform_delegate))
// From here, the callbacks and observers can be added.
#if BUILDFLAG(IS_ANDROID)
      ,
      browser_content_filters_observer_(
          content_filters_observer_bridge_factory.Run(
              kBrowserContentFiltersSettingName,
              base::BindRepeating(
                  &SupervisedUserService::EnableBrowserContentFilters,
                  base::Unretained(this)),
              base::BindRepeating(
                  &SupervisedUserService::DisableBrowserContentFilters,
                  base::Unretained(this)),
              base::BindRepeating(&IsSubjectToParentalControls,
                                  base::Unretained(user_prefs_)))),
      search_content_filters_observer_(
          content_filters_observer_bridge_factory.Run(
              kSearchContentFiltersSettingName,
              base::BindRepeating(
                  &SupervisedUserService::EnableSearchContentFilters,
                  base::Unretained(this)),
              base::BindRepeating(
                  &SupervisedUserService::DisableSearchContentFilters,
                  base::Unretained(this)),
              base::BindRepeating(&IsSubjectToParentalControls,
                                  base::Unretained(user_prefs_))))
#endif  // BUILDFLAG(IS_ANDROID)
{
  CHECK(settings_service_->IsReady())
      << "Settings service is initialized as part of the PrefService, which is "
         "a dependency of this service.";

#if BUILDFLAG(IS_ANDROID)
  browser_content_filters_observer_->Init();
  search_content_filters_observer_->Init();
#endif  // BUILDFLAG(IS_ANDROID)

  main_pref_change_registrar_.Init(&user_prefs_.get());
  main_pref_change_registrar_.Add(
      prefs::kSupervisedUserId,
      base::BindRepeating(&SupervisedUserService::OnSupervisedUserIdChanged,
                          base::Unretained(this)));

  OnSupervisedUserIdChanged();
}

void SupervisedUserService::SetSettingsServiceActive(bool active) {
  settings_service_->SetActive(active);

  // Trigger a sync reconfig to enable/disable the right SU data types.
  // The logic to do this lives in the
  // SupervisedUserSettingsDataTypeController.
  // TODO(crbug.com/40620346): Get rid of this hack and instead call
  // DataTypePreconditionChanged from the controller.
  if (sync_service_ &&
      sync_service_->GetUserSettings()->IsInitialSyncFeatureSetupComplete()) {
    // Trigger a reconfig by grabbing a SyncSetupInProgressHandle and
    // immediately releasing it again (via the temporary unique_ptr going away).
    std::ignore = sync_service_->GetSetupInProgressHandle();
  }
}

void SupervisedUserService::OnSupervisedUserIdChanged() {
  if (IsSubjectToParentalControls(user_prefs_.get())) {
    OnFamilyLinkParentalControlsEnabled();
  } else {
    OnFamilyLinkParentalControlsDisabled();
  }
}

void SupervisedUserService::OnFamilyLinkParentalControlsEnabled() {
  // If this trap catches change from AccountTrackerService, then this means
  // that the profile is being preloaded from disk or cache, but since the
  // browser's last use the status of family link parental controls and local
  // controls have changed. In this case it would be just enough to clear
  // browser's data. However, if this is triggered from ChildAccountService,
  // then it means that regular profile was turned to supervised while the
  // device was also locally supervised. In this case, next start of the browser
  // should be clean because it is expected that family link and device controls
  // are mutually exclusive and device controls are just being disabled.

#if BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(
          kSupervisedUserOverrideLocalSupervisionForFamilyLinkAccounts) &&
      IsSupervisedLocally()) {
    base::UmaHistogramEnumeration(kSupervisionConflictHistogramName,
                                  SupervisionHasConflict::kHasConflict);
    // This properly shuts down local supervision before enabling family link
    // supervision.
    browser_content_filters_observer_->OnChange(/*env=*/nullptr,
                                                /*enabled=*/false);
    search_content_filters_observer_->OnChange(/*env=*/nullptr,
                                               /*enabled=*/false);
  }
#endif  // BUILDFLAG(IS_ANDROID)

  CHECK(!IsSupervisedLocally())
      << "Family link parental controls cannot be manipulated when locally "
         "supervised.";

  // Remove the handlers of the disabled parental controls mode.
  RemoveURLFilterPrefChangeHandlers();

  // Also disables incognito mode.
  SetSettingsServiceActive(true);
  // TODO(crbug.com/447414264): Check if tabs should be closed in the first
  // place.
  platform_delegate_->CloseIncognitoTabs();

  remote_web_approvals_manager_.AddApprovalRequestCreator(
      std::make_unique<PermissionRequestCreatorImpl>(identity_manager_,
                                                     url_loader_factory_));

  // Add handlers at the end to avoid multiple notifications.
  AddCustodianPrefChangeHandlers();
  AddURLFilterPrefChangeHandlers();

  // Synchronize the filter.
  UpdateURLFilter();
}

void SupervisedUserService::OnFamilyLinkParentalControlsDisabled() {
  // Start with removing handlers, to avoid multiple notifications from pref
  // status changes from the settings service.
  RemoveURLFilterPrefChangeHandlers();
  RemoveCustodianPrefChangeHandlers();

  // All disabling operations are idempotent.
  SetSettingsServiceActive(false);
  remote_web_approvals_manager_.ClearApprovalRequestsCreators();

  // Synchronize the filter.
  UpdateURLFilter();
}

void SupervisedUserService::AddURLFilterPrefChangeHandlers() {
  url_filter_pref_change_registrar_.Init(&user_prefs_.get());
  for (const char* const pref : kUrlFilterSettingsPrefs) {
    url_filter_pref_change_registrar_.Add(
        pref, base::BindRepeating(&SupervisedUserService::OnURLFilterChanged,
                                  base::Unretained(this)));
  }
}
void SupervisedUserService::AddURLFilterPrefChangeSentinels() {
  url_filter_pref_change_registrar_.Init(&user_prefs_.get());
  for (const char* const pref : kUrlFilterSettingsPrefs) {
    url_filter_pref_change_registrar_.Add(
        pref, base::BindRepeating(&PrefChangeNotAllowed));
  }
}

void SupervisedUserService::AddCustodianPrefChangeHandlers() {
  custodian_pref_change_registrar_.Init(&user_prefs_.get());
  for (const auto* const pref : kCustodianInfoPrefs) {
    custodian_pref_change_registrar_.Add(
        pref,
        base::BindRepeating(&SupervisedUserService::OnCustodianInfoChanged,
                            base::Unretained(this)));
  }
}

void SupervisedUserService::RemoveURLFilterPrefChangeHandlers() {
  url_filter_pref_change_registrar_.RemoveAll();
}

void SupervisedUserService::RemoveCustodianPrefChangeHandlers() {
  custodian_pref_change_registrar_.RemoveAll();
}

void SupervisedUserService::OnCustodianInfoChanged() {
  observer_list_.Notify(&SupervisedUserServiceObserver::OnCustodianInfoChanged);
}

void SupervisedUserService::OnURLFilterChanged(const std::string& pref_name) {
  CHECK(IsSubjectToParentalControls(user_prefs_.get()))
      << "Url filter setting `" << pref_name
      << "` can only be dynamically changed by managed user infrastructure.";
  UpdateURLFilter(pref_name);
}

void SupervisedUserService::UpdateURLFilter(
    std::optional<std::string> pref_name) {
  // These prefs hold complex data structures that need to be updated.
  if (pref_name.value_or(prefs::kSupervisedUserManualHosts) ==
      prefs::kSupervisedUserManualHosts) {
    url_filter_->UpdateManualHosts();
  }
  if (pref_name.value_or(prefs::kSupervisedUserManualURLs) ==
      prefs::kSupervisedUserManualURLs) {
    url_filter_->UpdateManualUrls();
  }

  observer_list_.Notify(&SupervisedUserServiceObserver::OnURLFilterChanged);
}

void SupervisedUserService::Shutdown() {
  DCHECK(!did_shutdown_);
  did_shutdown_ = true;

#if BUILDFLAG(IS_ANDROID)
  browser_content_filters_observer_->Shutdown();
  search_content_filters_observer_->Shutdown();
#endif  // BUILDFLAG(IS_ANDROID)

  if (IsSubjectToParentalControls(user_prefs_.get())) {
    base::RecordAction(UserMetricsAction("ManagedUsers_QuitBrowser"));
  }

  CHECK(settings_service_->IsReady())
      << "This service depends on the settings service, which will be shut "
         "down in its own procedure";
  // Note: we can't shut down the settings service here, because it could put
  // the system in incorrect state: supervision is enabled, but artificially
  // deactivated settings service had also reset the filter to defaults (that
  // allow all url classifications). On the other hand, if supervision is
  // disabled, then the settings service is already inactive.
}

#if BUILDFLAG(IS_ANDROID)

namespace {
bool IsEligibleForContentFilters(const PrefService& user_prefs) {
  return !IsSubjectToParentalControls(user_prefs);
}
}  // namespace

void SupervisedUserService::EnableSearchContentFilters() {
  if (!IsEligibleForContentFilters(user_prefs_.get())) {
    return;
  }

  settings_service_->SetSuspended(true);
  content_filters_service_->SetSearchFiltersEnabled(true);
  if (platform_delegate_->ShouldCloseIncognitoTabs()) {
    platform_delegate_->CloseIncognitoTabs();
  }

  // OnSearchContentFiltersChanged reattributes the synthetic field trial
  // groups and then reloads search pages.
  observer_list_.Notify(
      &SupervisedUserServiceObserver::OnSearchContentFiltersChanged);
  // Required to emit WebFilterType metrics.
  UpdateURLFilter();
}
void SupervisedUserService::DisableSearchContentFilters() {
  content_filters_service_->SetSearchFiltersEnabled(false);
  if (!IsSupervisedLocally()) {
    settings_service_->SetSuspended(false);
  }

  // OnSearchContentFiltersChanged reattributes the synthetic field trial
  // groups and then reloads search pages.
  observer_list_.Notify(
      &SupervisedUserServiceObserver::OnSearchContentFiltersChanged);
}
void SupervisedUserService::EnableBrowserContentFilters() {
  if (!IsEligibleForContentFilters(user_prefs_.get())) {
    return;
  }

  RemoveURLFilterPrefChangeHandlers();
  settings_service_->SetSuspended(true);
  content_filters_service_->SetBrowserFiltersEnabled(true);
  if (platform_delegate_->ShouldCloseIncognitoTabs()) {
    platform_delegate_->CloseIncognitoTabs();
  }

  // Add handlers that will prevent unsupported url filter changes.
  AddURLFilterPrefChangeSentinels();

  // OnBrowserContentFiltersChanged reattributes the synthetic field trial
  // groups
  observer_list_.Notify(
      &SupervisedUserServiceObserver::OnBrowserContentFiltersChanged);
  // Required to emit WebFilterType metrics and reclassifies the observed
  // navigations.
  UpdateURLFilter();
}

void SupervisedUserService::DisableBrowserContentFilters() {
  RemoveURLFilterPrefChangeHandlers();
  content_filters_service_->SetBrowserFiltersEnabled(false);
  if (!IsSupervisedLocally()) {
    settings_service_->SetSuspended(false);
  }

  // OnBrowserContentFiltersChanged reattributes the synthetic field trial
  // groups
  observer_list_.Notify(
      &SupervisedUserServiceObserver::OnBrowserContentFiltersChanged);
  // Required to emit WebFilterType metrics and reclassifies the observed
  // navigations.
  UpdateURLFilter();
}

ContentFiltersObserverBridge*
SupervisedUserService::browser_content_filters_observer() {
  return browser_content_filters_observer_.get();
}

ContentFiltersObserverBridge*
SupervisedUserService::search_content_filters_observer() {
  return search_content_filters_observer_.get();
}

#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace supervised_user
