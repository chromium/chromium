// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/supervised_user_pref_store.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/feed/core/shared_prefs/pref_names.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_value_map.h"
#include "components/safe_search_api/safe_search_util.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/supervised_user/core/browser/device_parental_controls.h"
#include "components/supervised_user/core/browser/family_link_settings_service.h"
#include "components/supervised_user/core/browser/supervised_user_utils.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_prefs.h"
#include "extensions/buildflags/buildflags.h"

namespace supervised_user {
namespace {

const char kSupervisionConflictHistogramName[] =
    "SupervisedUsers.FamilyLinkSupervisionConflict";
enum class SupervisionHasConflict : int {
  kNoConflict = 0,
  kHasConflict = 1,
  kMaxValue = kHasConflict,
};

struct FamilyLinkSettingsPrefMappingEntry {
  const char* settings_name;
  const char* pref_name;
};

FamilyLinkSettingsPrefMappingEntry kFamilyLinkSettingsPrefMapping[] = {
    {
        supervised_user::kSigninAllowed,
        prefs::kSigninAllowed,
    },
    {
        supervised_user::kSigninAllowedOnNextStartup,
        prefs::kSigninAllowedOnNextStartup,
    },
    {
        supervised_user::kSkipParentApprovalToInstallExtensions,
        prefs::kSkipParentApprovalToInstallExtensions,
    },
};

FamilyLinkSettingsPrefMappingEntry kFamilyLinkWebFilteringPrefMapping[] = {
    {
        supervised_user::kContentPackDefaultFilteringBehavior,
        prefs::kDefaultSupervisedUserFilteringBehavior,
    },
    {
        supervised_user::kContentPackManualBehaviorHosts,
        prefs::kSupervisedUserManualHosts,
    },
    {
        supervised_user::kContentPackManualBehaviorURLs,
        prefs::kSupervisedUserManualURLs,
    },
    {
        supervised_user::kSafeSitesEnabled,
        prefs::kSupervisedUserSafeSites,
    },

};

}  // namespace

void SetSupervisedUserPrefStoreDefaults(PrefValueMap& pref_values) {
  if (!base::FeatureList::IsEnabled(
          supervised_user::kSupervisedUserUseUrlFilteringService)) {
    pref_values.SetInteger(
        prefs::kDefaultSupervisedUserFilteringBehavior,
        static_cast<int>(supervised_user::FilteringBehavior::kAllow));
    pref_values.SetBoolean(prefs::kSupervisedUserSafeSites, true);
  }

  pref_values.SetBoolean(policy::policy_prefs::kHideWebStoreIcon, false);
  pref_values.SetBoolean(feed::prefs::kEnableSnippets, false);
  pref_values.SetInteger(
      policy::policy_prefs::kIncognitoModeAvailability,
      static_cast<int>(policy::IncognitoModeAvailability::kDisabled));
}
}  // namespace supervised_user

SupervisedUserPrefStore::SupervisedUserPrefStore() = default;

SupervisedUserPrefStore::SupervisedUserPrefStore(
    supervised_user::FamilyLinkSettingsService* family_link_settings_service,
    supervised_user::DeviceParentalControls& device_parental_controls) {
  Init(family_link_settings_service, device_parental_controls);
}

void SupervisedUserPrefStore::Init(
    supervised_user::FamilyLinkSettingsService* family_link_settings_service,
    supervised_user::DeviceParentalControls& device_parental_controls) {
  family_link_settings_service_ = family_link_settings_service->GetWeakPtr();

  family_link_settings_subscription_ =
      family_link_settings_service->SubscribeForSettingsChange(
          base::BindRepeating(&SupervisedUserPrefStore::OnNewSettingsAvailable,
                              base::Unretained(this)));

  device_parental_controls_subscription_ =
      device_parental_controls.Subscribe(base::BindRepeating(
          &SupervisedUserPrefStore::OnDeviceParentalControlsChanged,
          weak_factory_.GetWeakPtr()));

  // The FamilyLinkSettingsService must be created before the PrefStore, and
  // it will notify the PrefStore to destroy both subscriptions when it is shut
  // down.
  shutdown_subscription_ = family_link_settings_service->SubscribeForShutdown(
      base::BindRepeating(&SupervisedUserPrefStore::OnSettingsServiceShutdown,
                          base::Unretained(this)));
}

bool SupervisedUserPrefStore::GetValue(std::string_view key,
                                       const base::Value** value) const {
  return prefs_->GetValue(key, value);
}

base::DictValue SupervisedUserPrefStore::GetValues() const {
  return prefs_->AsDict();
}

void SupervisedUserPrefStore::AddObserver(PrefStore::Observer* observer) {
  observers_.AddObserver(observer);
}

void SupervisedUserPrefStore::RemoveObserver(PrefStore::Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool SupervisedUserPrefStore::HasObservers() const {
  return !observers_.empty();
}

bool SupervisedUserPrefStore::IsInitializationComplete() const {
  return !!prefs_;
}

SupervisedUserPrefStore::~SupervisedUserPrefStore() = default;

void SupervisedUserPrefStore::OnDeviceParentalControlsChanged(
    const supervised_user::DeviceParentalControls& device_parental_controls) {
  device_parental_controls_state_.is_web_filtering_enabled =
      device_parental_controls.IsWebFilteringEnabled();
  device_parental_controls_state_.is_incognito_mode_disabled =
      device_parental_controls.IsIncognitoModeDisabled();
  device_parental_controls_state_.is_safe_search_forced =
      device_parental_controls.IsSafeSearchForced();
  device_parental_controls_state_.is_enabled =
      device_parental_controls.IsEnabled();
  RecreatePreferences();
}

void SupervisedUserPrefStore::OnNewSettingsAvailable(
    const base::DictValue& settings) {
  family_link_settings_ = settings.Clone();
  RecreatePreferences();
}

void SupervisedUserPrefStore::RecreatePreferences() {
  // Ignore notifications about device parental controls settings which are sent
  // unconditionally until the family link settings are ready (have emitted at
  // least one notification).
  if (!family_link_settings_.has_value()) {
    return;
  }

  std::unique_ptr<PrefValueMap> old_prefs = std::move(prefs_);
  prefs_ = std::make_unique<PrefValueMap>();

  bool is_family_link_settings_service_active =
      family_link_settings_service_ &&
      family_link_settings_service_->IsActive();

  if (is_family_link_settings_service_active) {
    supervised_user::SetSupervisedUserPrefStoreDefaults(*prefs_.get());

#if BUILDFLAG(IS_ANDROID)
    syncer::SyncPrefs::SetTypeDisabledByCustodian(
        prefs_.get(), syncer::UserSelectableType::kPayments);
#endif

    // Copy non-web filtering family link user settings to prefs.
    for (const auto& entry : supervised_user::kFamilyLinkSettingsPrefMapping) {
      const base::Value* value =
          family_link_settings_->Find(entry.settings_name);
      if (value) {
        prefs_->SetValue(entry.pref_name, value->Clone());
      }
    }

    // TODO(crbug.com/465666839): after the migration is complete, stop
    // propagating these prefs (guard with kSupervisedUserUseUrlFilteringService
    // first).
    for (const auto& entry :
         supervised_user::kFamilyLinkWebFilteringPrefMapping) {
      const base::Value* value =
          family_link_settings_->Find(entry.settings_name);
      if (value) {
        prefs_->SetValue(entry.pref_name, value->Clone());
      }
    }

#if BUILDFLAG(ENABLE_EXTENSIONS)
    {
      bool permissions_disallowed =
          family_link_settings_->FindBool(supervised_user::kGeolocationDisabled)
              .value_or(false);
      prefs_->SetBoolean(prefs::kSupervisedUserExtensionsMayRequestPermissions,
                         !permissions_disallowed);
    }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

    // Apparently two parental controls systems are enabled at the same time.
    // This is considered a conflict which in versions before
    // kSupervisedUserUseUrlFilteringService and
    // kSupervisedUserMergeDeviceParentalControlsAndFamilyLinkPrefs was resolved
    // in favor of Family Link settings.
    if (device_parental_controls_state_.is_enabled) {
      base::UmaHistogramEnumeration(
          supervised_user::kSupervisionConflictHistogramName,
          supervised_user::SupervisionHasConflict::kHasConflict);
    }
  }

  // Merge device parental controls settings with the Family Link settings in
  // one of two situations:
  // 1. Family Link is not enabled (old behavior).
  // 2. Family Link is enabled, and new merging feature is enabled too.
  // The merge policy is to select the most restrictive setting.
  if (!is_family_link_settings_service_active ||
      base::FeatureList::IsEnabled(
          supervised_user::
              kSupervisedUserMergeDeviceParentalControlsAndFamilyLinkPrefs)) {
    if (device_parental_controls_state_.is_incognito_mode_disabled) {
      // IncognitoModeAvailability::kDisabled is the most restrictive setting -
      // it's safe to apply it regardless of the Family Link settings.
      prefs_->SetInteger(
          policy::policy_prefs::kIncognitoModeAvailability,
          static_cast<int>(policy::IncognitoModeAvailability::kDisabled));
    }
    if (device_parental_controls_state_.is_safe_search_forced) {
      // kForceGoogleSafeSearch=true is also the most restrictive setting.
      prefs_->SetBoolean(policy::policy_prefs::kForceGoogleSafeSearch, true);
    }
  }

  // Web filtering prefs are being deprecated: only merge them if device
  // parental controls are mutually exclusive with family link settings and new
  // way of delivering the web filtering settings is not enabled.
  if (!is_family_link_settings_service_active &&
      !base::FeatureList::IsEnabled(
          supervised_user::kSupervisedUserUseUrlFilteringService)) {
    if (device_parental_controls_state_.is_web_filtering_enabled) {
      prefs_->SetBoolean(prefs::kSupervisedUserSafeSites, true);
    }
  }

  // Unset `old_prefs` means that this is the first notification from the
  // supervised user (Family Link) settings service.
  if (!old_prefs) {
    // If this is the first notification from the settings service, notify
    // observers about initialization completion.
    observers_.Notify(&PrefStore::Observer::OnInitializationCompleted, true);

    // Set `old_prefs` to an empty value to fulfill the contract of
    // `NotifyObserversAboutChanges()`.
    old_prefs = std::make_unique<PrefValueMap>();
  }

  NotifyObserversAboutChanges(std::move(old_prefs));
}

void SupervisedUserPrefStore::NotifyObserversAboutChanges(
    std::unique_ptr<PrefValueMap> diff_base) {
  std::vector<std::string> changed_prefs;
  prefs_->GetDifferingKeys(diff_base.get(), &changed_prefs);

  // Send out change notifications.
  for (const std::string& pref : changed_prefs) {
    observers_.Notify(&PrefStore::Observer::OnPrefValueChanged, pref);
  }
}

void SupervisedUserPrefStore::OnSettingsServiceShutdown() {
  family_link_settings_subscription_ = {};
  shutdown_subscription_ = {};
}
