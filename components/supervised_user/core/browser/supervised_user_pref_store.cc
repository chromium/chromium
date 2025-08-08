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
#include "base/metrics/histogram_functions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/feed/core/shared_prefs/pref_names.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_value_map.h"
#include "components/safe_search_api/safe_search_util.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/supervised_user/core/browser/supervised_user_content_filters_service.h"
#include "components/supervised_user/core/browser/supervised_user_settings_service.h"
#include "components/supervised_user/core/browser/supervised_user_url_filter.h"
#include "components/supervised_user/core/browser/supervised_user_utils.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_prefs.h"
#include "extensions/buildflags/buildflags.h"

namespace supervised_user {
namespace {

struct SupervisedUserSettingsPrefMappingEntry {
  const char* settings_name;
  const char* pref_name;
};

SupervisedUserSettingsPrefMappingEntry kSupervisedUserSettingsPrefMapping[] = {
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

}  // namespace

void SetSupervisedUserPrefStoreDefaults(PrefValueMap& pref_values) {
  pref_values.SetInteger(
      prefs::kDefaultSupervisedUserFilteringBehavior,
      static_cast<int>(supervised_user::FilteringBehavior::kAllow));

  pref_values.SetBoolean(policy::policy_prefs::kHideWebStoreIcon, false);
  pref_values.SetBoolean(feed::prefs::kEnableSnippets, false);
  pref_values.SetBoolean(prefs::kSupervisedUserSafeSites, true);
}
}  // namespace supervised_user

SupervisedUserPrefStore::SupervisedUserPrefStore() = default;

SupervisedUserPrefStore::SupervisedUserPrefStore(
    supervised_user::SupervisedUserSettingsService*
        supervised_user_settings_service,
    supervised_user::SupervisedUserContentFiltersService*
        content_filters_service) {
  Init(supervised_user_settings_service, content_filters_service);
}

void SupervisedUserPrefStore::Init(
    supervised_user::SupervisedUserSettingsService*
        supervised_user_settings_service,
    supervised_user::SupervisedUserContentFiltersService*
        content_filters_service) {
  user_settings_subscription_ =
      supervised_user_settings_service->SubscribeForSettingsChange(
          base::BindRepeating(&SupervisedUserPrefStore::OnNewSettingsAvailable,
                              base::Unretained(this)));

  if (content_filters_service) {
    content_filter_settings_subscription_ =
        content_filters_service->SubscribeForContentFiltersStateChange(
            base::BindRepeating(
                &SupervisedUserPrefStore::OnNewContentFiltersStateAvailable,
                weak_factory_.GetWeakPtr()));
  }

  // The SupervisedUserSettingsService must be created before the PrefStore, and
  // it will notify the PrefStore to destroy both subscriptions when it is shut
  // down.
  shutdown_subscription_ =
      supervised_user_settings_service->SubscribeForShutdown(
          base::BindRepeating(
              &SupervisedUserPrefStore::OnSettingsServiceShutdown,
              base::Unretained(this)));
}

bool SupervisedUserPrefStore::GetValue(std::string_view key,
                                       const base::Value** value) const {
  return prefs_->GetValue(key, value);
}

base::Value::Dict SupervisedUserPrefStore::GetValues() const {
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

void SupervisedUserPrefStore::OnNewContentFiltersStateAvailable(
    supervised_user::SupervisedUserContentFiltersService::State state) {
  std::unique_ptr<PrefValueMap> old_prefs = std::move(prefs_);
  prefs_ = std::make_unique<PrefValueMap>();

  if (state.incognito_disabled) {
    prefs_->SetInteger(
        policy::policy_prefs::kIncognitoModeAvailability,
        static_cast<int>(policy::IncognitoModeAvailability::kDisabled));
  }
  if (state.safe_sites_enabled) {
    prefs_->SetBoolean(prefs::kSupervisedUserSafeSites, true);
  }
  if (state.safe_search_enabled) {
    prefs_->SetBoolean(policy::policy_prefs::kForceGoogleSafeSearch, true);
  }

  NotifyObserversAboutChanges(std::move(old_prefs));
}

void SupervisedUserPrefStore::OnNewSettingsAvailable(
    const base::Value::Dict& settings) {
  std::unique_ptr<PrefValueMap> old_prefs = std::move(prefs_);
  prefs_ = std::make_unique<PrefValueMap>();
  if (!settings.empty()) {
    supervised_user::SetSupervisedUserPrefStoreDefaults(*prefs_.get());

#if BUILDFLAG(IS_ANDROID)
    syncer::SyncPrefs::SetTypeDisabledByCustodian(
        prefs_.get(), syncer::UserSelectableType::kPayments);
#endif

    // Copy supervised user settings to prefs.
    for (const auto& entry :
         supervised_user::kSupervisedUserSettingsPrefMapping) {
      const base::Value* value = settings.Find(entry.settings_name);
      if (value) {
        prefs_->SetValue(entry.pref_name, value->Clone());
      }
    }

    // Manually set preferences that aren't direct copies of the settings value.
    {
      // Incognito is disabled for supervised users across platforms.
      // First-party sites use signed-in cookies to ensure that parental
      // restrictions are applied for Unicorn accounts.
      prefs_->SetInteger(
          policy::policy_prefs::kIncognitoModeAvailability,
          static_cast<int>(policy::IncognitoModeAvailability::kDisabled));
    }

#if BUILDFLAG(ENABLE_EXTENSIONS)
    {
      bool permissions_disallowed =
          settings.FindBool(supervised_user::kGeolocationDisabled)
              .value_or(false);
      prefs_->SetBoolean(prefs::kSupervisedUserExtensionsMayRequestPermissions,
                         !permissions_disallowed);
    }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
  }

  if (!old_prefs) {
    for (Observer& observer : observers_) {
      observer.OnInitializationCompleted(true);
    }
    return;
  }

  NotifyObserversAboutChanges(std::move(old_prefs));
}

void SupervisedUserPrefStore::NotifyObserversAboutChanges(
    std::unique_ptr<PrefValueMap> diff_base) {
  std::vector<std::string> changed_prefs;
  prefs_->GetDifferingKeys(diff_base.get(), &changed_prefs);

  // Send out change notifications.
  for (const std::string& pref : changed_prefs) {
    for (Observer& observer : observers_) {
      observer.OnPrefValueChanged(pref);
    }
  }
}

void SupervisedUserPrefStore::OnSettingsServiceShutdown() {
  user_settings_subscription_ = {};
  shutdown_subscription_ = {};
}
