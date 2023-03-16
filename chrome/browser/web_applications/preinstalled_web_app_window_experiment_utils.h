// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_PREINSTALLED_WEB_APP_WINDOW_EXPERIMENT_UTILS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_PREINSTALLED_WEB_APP_WINDOW_EXPERIMENT_UTILS_H_

#include "base/containers/flat_set.h"
#include "base/metrics/field_trial_params.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom-forward.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/common/chrome_features.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class PrefService;

namespace apps {
enum class DefaultAppName;
}

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace web_app {

class WebAppRegistrar;

// Utility functions for managing prefs and checks for
// `PreinstalledWebAppWindowExperiment`.
namespace preinstalled_web_app_window_experiment_utils {

const base::FeatureParam<features::PreinstalledWebAppWindowExperimentUserGroup>&
GetFeatureParam();

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

void DeleteExperimentPrefs(PrefService* pref_service);

// User group:

absl::optional<mojom::UserDisplayMode> UserGroupToUserDisplayMode(
    features::PreinstalledWebAppWindowExperimentUserGroup user_group);

// Returns the current value of the experiment parameter.
features::PreinstalledWebAppWindowExperimentUserGroup GetUserGroup();

// Returns the persisted value of the experiment parameter after eligibility has
// been checked, or `kUnknown` if not set.
features::PreinstalledWebAppWindowExperimentUserGroup GetUserGroupPref(
    PrefService* pref_service);

void SetUserGroupPref(
    PrefService* pref_service,
    features::PreinstalledWebAppWindowExperimentUserGroup user_group);

// Eligibility:

// Returns the persisted value for whether the user is eligible for the
// experiment, or nullopt if not set.
absl::optional<bool> GetEligibilityPref(const PrefService* pref_service);

void SetEligibilityPref(PrefService* pref_service, bool eligible);

// Returns whether the user is currently eligible to join the experiment based
// on installed apps state.
bool DetermineEligibility(WebAppRegistrar& registrar);

// Apps launched before experiment:

// Returns whether the given preinstalled app was launched before the
// experiment began.
bool HasLaunchedAppBeforeExperiment(const AppId& preinstalled_app_id,
                                    PrefService* pref_service);

void SetHasLaunchedAppsBeforePref(PrefService* pref_service,
                                  const base::flat_set<AppId>& app_ids);

// Display mode:

base::flat_set<AppId> GetAppIdsWithUserOverridenDisplayModePref(
    PrefService* pref_service);

void SetUserOverridenDisplayModePref(PrefService* pref_service,
                                     const AppId& app_id);

// Histograms:

void RecordDisplayModeChangeHistogram(PrefService* pref_service,
                                      mojom::UserDisplayMode display_mode,
                                      apps::DefaultAppName app);

void RecordLinkCapturingChangeHistogram(PrefService* pref_service,
                                        bool open_in_app,
                                        apps::DefaultAppName app);

}  // namespace preinstalled_web_app_window_experiment_utils

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_PREINSTALLED_WEB_APP_WINDOW_EXPERIMENT_UTILS_H_
