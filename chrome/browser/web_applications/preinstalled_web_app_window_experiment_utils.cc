// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/preinstalled_web_app_window_experiment_utils.h"

#include <string>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/checked_iterators.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/clamped_math.h"
#include "base/time/time.h"
#include "base/values.h"
// TODO(crbug.com/1402146): Allow web apps to depend on app service.
#include "chrome/browser/apps/app_service/metrics/app_service_metrics.h"  // nogncheck
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom-shared.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace web_app::preinstalled_web_app_window_experiment_utils {

using mojom::UserDisplayMode;
using UserGroup = features::PreinstalledWebAppWindowExperimentUserGroup;

// Note these strings must match those in
// `kPreinstalledWebAppWindowExperimentVariations` in about_flags.cc.
constexpr base::FeatureParam<UserGroup>::Option kUserGroupParamOptions[] = {
    {UserGroup::kUnknown, "unknown"},
    {UserGroup::kControl, "control"},
    {UserGroup::kWindow, "window"},
    {UserGroup::kTab, "tab"}};

constexpr base::FeatureParam<UserGroup> kUserGroupParam{
    &features::kPreinstalledWebAppWindowExperiment, "user_group",
    UserGroup::kUnknown, &kUserGroupParamOptions};

const base::FeatureParam<UserGroup>& GetFeatureParam() {
  return kUserGroupParam;
}

// Dictionary for persisted experiment values. Prefs layout:
// web_apps: {
//   preinstalled_app_window_experiment: {
//     eligible: true|false,
//     user_group: "control"|"window"|"tab",
//     app_ids_launched_before_experiment: ["abc123", "def456"],
//     app_ids_with_user_overridden_display_mode: ["abc123", "fed123"],
//   }
// }
const char kWebAppPreinstalledAppWindowExperimentPref[] =
    "web_apps.preinstalled_app_window_experiment";
const char kEligiblePrefKey[] = "eligible";
const char kUserGroupPrefKey[] = "user_group";
const char kAppIdsLaunchedBeforePrefKey[] =
    "web_apps.preinstalled_app_window_experiment.app_ids_launched_before_"
    "experiment";
const char kAppIdsWithUserOverriddenDisplayModePrefKey[] =
    "web_apps.preinstalled_app_window_experiment.app_ids_with_user_overridden_"
    "display_mode";

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  // TODO(crbug.com/1385246): Delete prefs when experiment ends (likely by end
  // of 2023).
  registry->RegisterDictionaryPref(kWebAppPreinstalledAppWindowExperimentPref);
  registry->RegisterListPref(kAppIdsLaunchedBeforePrefKey);
  registry->RegisterListPref(kAppIdsWithUserOverriddenDisplayModePrefKey);
}

void DeleteExperimentPrefs(PrefService* pref_service) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  pref_service->ClearPref(kWebAppPreinstalledAppWindowExperimentPref);
}

///////////////////////////////////////////////////////////////////////////////
// User group:
///////////////////////////////////////////////////////////////////////////////

absl::optional<UserDisplayMode> UserGroupToUserDisplayMode(
    UserGroup user_group) {
  switch (user_group) {
    case UserGroup::kUnknown:
    case UserGroup::kControl:
      return absl::nullopt;
    case UserGroup::kWindow:
      return UserDisplayMode::kStandalone;
    case UserGroup::kTab:
      return UserDisplayMode::kBrowser;
  }
}

UserGroup GetUserGroup() {
  if (!base::FeatureList::IsEnabled(
          features::kPreinstalledWebAppWindowExperiment)) {
    return UserGroup::kUnknown;
  }
  return kUserGroupParam.Get();
}

UserGroup GetUserGroupPref(PrefService* pref_service) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const base::Value::Dict& experiment_prefs =
      pref_service->GetDict(kWebAppPreinstalledAppWindowExperimentPref);
  const std::string* user_group_name =
      experiment_prefs.FindString(kUserGroupPrefKey);
  if (!user_group_name) {
    return UserGroup::kUnknown;
  }

  for (auto& option : kUserGroupParamOptions) {
    if (option.name == *user_group_name) {
      return option.value;
    }
  }

  return UserGroup::kUnknown;
}

void SetUserGroupPref(PrefService* pref_service, UserGroup user_group) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(user_group != UserGroup::kUnknown);
  ScopedDictPrefUpdate update(pref_service,
                              kWebAppPreinstalledAppWindowExperimentPref);
  update->Set(kUserGroupPrefKey, kUserGroupParam.GetName(user_group));
}

///////////////////////////////////////////////////////////////////////////////
// Eligibility:
///////////////////////////////////////////////////////////////////////////////

absl::optional<bool> GetEligibilityPref(const PrefService* pref_service) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const base::Value::Dict& experiment_prefs =
      pref_service->GetDict(kWebAppPreinstalledAppWindowExperimentPref);
  return experiment_prefs.FindBool(kEligiblePrefKey);
}

void SetEligibilityPref(PrefService* pref_service, bool eligible) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ScopedDictPrefUpdate update(pref_service,
                              kWebAppPreinstalledAppWindowExperimentPref);
  update->Set(kEligiblePrefKey, eligible);
}

// Arbitrary fixed time cutoff to consider installations "recent". Corresponds
// to approximately 2023-03-01.
constexpr base::Time kOldestAllowedInstallTime =
    base::Time::UnixEpoch() + (base::Days(365) * 53) + base::Days(60);

bool AllWebAppsInstalledRecently(WebAppRegistrar& registrar) {
  for (const WebApp& web_app : registrar.GetApps()) {
    // Some old web apps may not have an install_time set.
    if (web_app.install_time().is_null() ||
        web_app.install_time() < kOldestAllowedInstallTime) {
      return false;
    }
  }
  return true;
}

bool AllWebAppsHaveNonSyncInstallSurface(WebAppRegistrar& registrar) {
  for (const WebApp& web_app : registrar.GetApps()) {
    // Use `latest_install_source` not `GetSources` because we want the
    // install surface, not the source of app management
    auto source = web_app.latest_install_source();
    if (!source.has_value() || *source == webapps::WebappInstallSource::SYNC) {
      return false;
    }
  }
  return true;
}

bool AppsAreInstallingFromSync(WebAppRegistrar& registrar) {
  for (const WebApp& web_app : registrar.GetAppsIncludingStubs()) {
    if (web_app.is_from_sync_and_pending_installation()) {
      return true;
    }
  }
  return false;
}

bool DetermineEligibility(WebAppRegistrar& registrar) {
  return AllWebAppsInstalledRecently(registrar) &&
         AllWebAppsHaveNonSyncInstallSurface(registrar) &&
         !AppsAreInstallingFromSync(registrar);
}

///////////////////////////////////////////////////////////////////////////////
// Apps launched before experiment:
///////////////////////////////////////////////////////////////////////////////

bool HasLaunchedAppBeforeExperiment(const AppId& app_id,
                                    PrefService* pref_service) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const base::Value::List& app_ids_launched_before =
      pref_service->GetList(kAppIdsLaunchedBeforePrefKey);

  return base::Contains(app_ids_launched_before, app_id);
}

void SetHasLaunchedAppsBeforePref(
    PrefService* pref_service,
    const base::flat_set<AppId>& preinstalled_apps_launched_before) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  ScopedListPrefUpdate update(pref_service, kAppIdsLaunchedBeforePrefKey);
  update->clear();
  for (const AppId& app_id : preinstalled_apps_launched_before) {
    update->Append(app_id);
  }
}

///////////////////////////////////////////////////////////////////////////////
// Display mode:
///////////////////////////////////////////////////////////////////////////////

base::flat_set<AppId> GetAppIdsWithUserOverridenDisplayModePref(
    PrefService* pref_service) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const base::Value::List& user_overridden_app_ids =
      pref_service->GetList(kAppIdsWithUserOverriddenDisplayModePrefKey);

  std::vector<AppId> app_ids;
  for (auto& app_id_value : user_overridden_app_ids) {
    if (app_id_value.is_string()) {
      app_ids.push_back(app_id_value.GetString());
    }
  }
  return app_ids;
}

// Add `app_id` to list of apps with user-overridden display mode.
void SetUserOverridenDisplayModePref(PrefService* pref_service,
                                     const AppId& app_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  ScopedListPrefUpdate update(pref_service,
                              kAppIdsWithUserOverriddenDisplayModePrefKey);
  if (!base::Contains(update.Get(), app_id)) {
    update->Append(app_id);
  }
}

///////////////////////////////////////////////////////////////////////////////
// Histograms:
///////////////////////////////////////////////////////////////////////////////

const char* kControlToWindowHistogram =
    "WebApp.Preinstalled.WindowExperiment.Control.ChangedToWindow";
const char* kControlToTabHistogram =
    "WebApp.Preinstalled.WindowExperiment.Control.ChangedToTab";
const char* kTabToWindowHistogram =
    "WebApp.Preinstalled.WindowExperiment.Tab.ChangedToWindow";
const char* kTabToTabHistogram =
    "WebApp.Preinstalled.WindowExperiment.Tab.ChangedToTab";
const char* kWindowToWindowHistogram =
    "WebApp.Preinstalled.WindowExperiment.Window.ChangedToWindow";
const char* kWindowToTabHistogram =
    "WebApp.Preinstalled.WindowExperiment.Window.ChangedToTab";

void RecordDisplayModeChangeHistogram(PrefService* pref_service,
                                      UserDisplayMode display_mode,
                                      apps::DefaultAppName app) {
  // Tabbed display mode is not launched.
  DCHECK_NE(display_mode, UserDisplayMode::kTabbed);

  // Use the persisted UserGroup instead of the feature param, in case the
  // experiment configuration changes.
  UserGroup user_group = GetUserGroupPref(pref_service);
  // We should only be recording if the experiment is active.
  DCHECK_NE(user_group, UserGroup::kUnknown);

  switch (user_group) {
    case UserGroup::kControl:
      switch (display_mode) {
        case UserDisplayMode::kStandalone:
          base::UmaHistogramEnumeration(kControlToWindowHistogram, app);
          return;
        case UserDisplayMode::kBrowser:
          base::UmaHistogramEnumeration(kControlToTabHistogram, app);
          return;
        case UserDisplayMode::kTabbed:
          return;
      }
    case UserGroup::kTab:
      switch (display_mode) {
        case UserDisplayMode::kStandalone:
          base::UmaHistogramEnumeration(kTabToWindowHistogram, app);
          return;
        case UserDisplayMode::kBrowser:
          base::UmaHistogramEnumeration(kTabToTabHistogram, app);
          return;
        case UserDisplayMode::kTabbed:
          return;
      }
    case UserGroup::kWindow:
      switch (display_mode) {
        case UserDisplayMode::kStandalone:
          base::UmaHistogramEnumeration(kWindowToWindowHistogram, app);
          return;
        case UserDisplayMode::kBrowser:
          base::UmaHistogramEnumeration(kWindowToTabHistogram, app);
          return;
        case UserDisplayMode::kTabbed:
          return;
      }
    case UserGroup::kUnknown:
      return;
  }
}

const char* kControlLinkCapturingEnabledHistogram =
    "WebApp.Preinstalled.WindowExperiment.Control.LinkCapturingEnabled";
const char* kControlLinkCapturingDisabledHistogram =
    "WebApp.Preinstalled.WindowExperiment.Control.LinkCapturingDisabled";
const char* kTabLinkCapturingEnabledHistogram =
    "WebApp.Preinstalled.WindowExperiment.Tab.LinkCapturingEnabled";
const char* kTabLinkCapturingDisabledHistogram =
    "WebApp.Preinstalled.WindowExperiment.Tab.LinkCapturingDisabled";
const char* kWindowLinkCapturingEnabledHistogram =
    "WebApp.Preinstalled.WindowExperiment.Window.LinkCapturingEnabled";
const char* kWindowLinkCapturingDisabledHistogram =
    "WebApp.Preinstalled.WindowExperiment.Window.LinkCapturingDisabled";

void RecordLinkCapturingChangeHistogram(PrefService* pref_service,
                                        bool open_in_app,
                                        apps::DefaultAppName app) {
  // Use the persisted UserGroup instead of the feature param, in case the
  // experiment configuration changes.
  UserGroup user_group = GetUserGroupPref(pref_service);
  // We should only be recording if the experiment is active.
  DCHECK_NE(user_group, UserGroup::kUnknown);

  switch (user_group) {
    case UserGroup::kControl:
      if (open_in_app) {
        base::UmaHistogramEnumeration(kControlLinkCapturingEnabledHistogram,
                                      app);
      } else {
        base::UmaHistogramEnumeration(kControlLinkCapturingDisabledHistogram,
                                      app);
      }
      return;
    case UserGroup::kTab:
      if (open_in_app) {
        base::UmaHistogramEnumeration(kTabLinkCapturingEnabledHistogram, app);
      } else {
        base::UmaHistogramEnumeration(kTabLinkCapturingDisabledHistogram, app);
      }
      return;
    case UserGroup::kWindow:
      if (open_in_app) {
        base::UmaHistogramEnumeration(kWindowLinkCapturingEnabledHistogram,
                                      app);
      } else {
        base::UmaHistogramEnumeration(kWindowLinkCapturingDisabledHistogram,
                                      app);
      }
      return;
    case UserGroup::kUnknown:
      return;
  }
}

}  //  namespace web_app::preinstalled_web_app_window_experiment_utils
