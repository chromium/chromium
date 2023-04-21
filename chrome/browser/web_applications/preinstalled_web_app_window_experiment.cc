// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/preinstalled_web_app_window_experiment.h"

#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "build/build_config.h"
// TODO(crbug.com/1402146): Allow web apps to depend on app service.
#include "chrome/browser/apps/app_service/app_service_proxy.h"  // nogncheck
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"  // nogncheck
#include "chrome/browser/apps/app_service/metrics/app_service_metrics.h"  // nogncheck
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom-shared.h"
#include "chrome/browser/web_applications/preinstalled_web_app_window_experiment_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/common/chrome_features.h"
#include "components/prefs/pref_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace web_app {

using mojom::UserDisplayMode;
using UserGroup = features::PreinstalledWebAppWindowExperimentUserGroup;

namespace {

namespace utils = preinstalled_web_app_window_experiment_utils;

base::flat_set<AppId> GetLaunchedPreinstalledAppIds(
    WebAppRegistrar& registrar) {
  std::vector<AppId> app_ids;
  for (const WebApp& web_app : registrar.GetApps()) {
    if (web_app.IsPreinstalledApp() && !web_app.last_launch_time().is_null()) {
      app_ids.push_back(web_app.app_id());
    }
  }
  return app_ids;
}

std::vector<AppId> SetSupportedLinksPreferenceForPreinstalledApps(
    WebAppRegistrar& registrar,
    apps::AppServiceProxy& proxy) {
  std::vector<AppId> apps_affected;
  for (const WebApp& web_app : registrar.GetApps()) {
    if (web_app.IsPreinstalledApp()) {
      proxy.SetSupportedLinksPreference(web_app.app_id());
      apps_affected.push_back(web_app.app_id());
    }
  }
  return apps_affected;
}

void SetUserDisplayModeOverridesForPreinstalledAppsOnRegistrar(
    WebAppRegistrar& registrar,
    PrefService* pref_service,
    bool notify_all) {
  UserGroup user_group_pref = utils::GetUserGroupPref(pref_service);
  auto opt_display_mode = utils::UserGroupToUserDisplayMode(user_group_pref);
  if (!opt_display_mode.has_value()) {
    // No overrides to apply unless pref maps to `kBrowser` or `kStandalone`.
    return;
  }
  UserDisplayMode display_mode = opt_display_mode.value();

  // Exclude any apps for which the user has explicitly set a display mode.
  base::flat_set<AppId> user_set_apps =
      utils::GetAppIdsWithUserOverridenDisplayModePref(pref_service);

  std::vector<std::pair<AppId, UserDisplayMode>> overrides;
  for (const WebApp& web_app : registrar.GetApps()) {
    if (web_app.IsPreinstalledApp() &&
        !user_set_apps.contains(web_app.app_id())) {
      overrides.emplace_back(web_app.app_id(), display_mode);
    }
  }

  registrar.SetUserDisplayModeOverridesForExperiment(std::move(overrides));

  if (!notify_all) {
    return;
  }

  for (const WebApp& web_app : registrar.GetApps()) {
    if (web_app.IsPreinstalledApp() &&
        !user_set_apps.contains(web_app.app_id())) {
      registrar.NotifyWebAppUserDisplayModeChanged(web_app.app_id(),
                                                   display_mode);
    }
  }
}

void PersistStateFromPrefsToWebAppDb(PrefService* pref_service,
                                     WebAppProvider& provider) {
  UserGroup user_group = utils::GetUserGroupPref(pref_service);
  auto opt_display_mode = utils::UserGroupToUserDisplayMode(user_group);
  if (!opt_display_mode.has_value()) {
    // Nothing to persist unless pref maps to `kBrowser` or `kStandalone`.
    return;
  }

  // Set all default apps to the experiment display mode, unless the user has
  // manually set the display mode for that app.
  base::flat_set<AppId> user_set_apps =
      utils::GetAppIdsWithUserOverridenDisplayModePref(pref_service);
  std::vector<AppId> experiment_overrides;
  for (const WebApp& web_app : provider.registrar_unsafe().GetApps()) {
    if (web_app.IsPreinstalledApp() &&
        !user_set_apps.contains(web_app.app_id())) {
      experiment_overrides.emplace_back(web_app.app_id());
    }
  }
  for (const AppId& app_id : experiment_overrides) {
    provider.scheduler().ScheduleCallbackWithLock(
        "PreinstalledWebAppWindowExperiment:PersistStateFromPrefsToWebAppDb",
        std::make_unique<AppLockDescription>(app_id),
        base::BindOnce(
            [](AppId app_id, mojom::UserDisplayMode display_mode,
               AppLock& lock) {
              lock.sync_bridge().SetAppUserDisplayMode(
                  app_id, display_mode,
                  /*is_user_action=*/false);
            },
            app_id, *opt_display_mode));
  }
}

}  // namespace

BASE_FEATURE(kWebAppWindowExperimentCleanup,
             "WebAppWindowExperimentCleanup",
             base::FEATURE_ENABLED_BY_DEFAULT);

PreinstalledWebAppWindowExperiment::PreinstalledWebAppWindowExperiment(
    Profile* profile)
    : profile_(profile) {
#if !BUILDFLAG(IS_CHROMEOS)
  NOTREACHED() << "PreinstalledWebAppWindowExperiment is CrOS only";
#endif
}

PreinstalledWebAppWindowExperiment::~PreinstalledWebAppWindowExperiment() =
    default;

void PreinstalledWebAppWindowExperiment::Start() {
  if (!WebAppProvider::GetForWebApps(profile_)) {
    // Don't run on ash-chrome when lacros is primary.
    return;
  }

  WebAppProvider::GetForWebApps(profile_)->on_registry_ready().Post(
      FROM_HERE,
      base::BindOnce(&PreinstalledWebAppWindowExperiment::CheckEligible,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PreinstalledWebAppWindowExperiment::CheckEligible() {
  if (utils::GetUserGroup() == UserGroup::kUnknown) {
    // Experiment has been disabled or was never enabled for this user.
    CleanUp();
    return;
  }

  // Use eligible pref to know if we need to do first time setup.
  absl::optional<bool> eligible_pref =
      utils::GetEligibilityPref(profile_->GetPrefs());
  if (!eligible_pref.has_value()) {
    FirstTimeSetup();
    return;
  }

  if (!eligible_pref.value()) {
    // Previously determined ineligible.
    setup_done_for_testing_.Signal();
    return;
  }

  StartOverridesAndObservations();
}

void PreinstalledWebAppWindowExperiment::NotifyPreinstalledAppsInstalled() {
  if (!preinstalled_apps_installed_.is_signaled()) {
    preinstalled_apps_installed_.Signal();
  }
}

void PreinstalledWebAppWindowExperiment::FirstTimeSetup() {
  // Wait for first sync and preinstalled app install before determining
  // eligibility and writing it to prefs, then start if eligible.
  base::RepeatingClosure barrier = base::BarrierClosure(
      /*num_closures=*/2,
      base::BindOnce(
          &PreinstalledWebAppWindowExperiment::SetFirstTimePrefsThenMaybeStart,
          weak_ptr_factory_.GetWeakPtr()));

  preinstalled_apps_installed_.Post(FROM_HERE, barrier);

  WebAppProvider::GetForWebApps(profile_)
      ->sync_bridge_unsafe()
      .on_sync_connected()
      .Post(FROM_HERE, barrier);
}

void PreinstalledWebAppWindowExperiment::SetFirstTimePrefsThenMaybeStart() {
  bool eligible = utils::DetermineEligibility(profile_, registrar_unsafe());
  utils::SetEligibilityPref(profile_->GetPrefs(), eligible);

  if (!eligible) {
    setup_done_for_testing_.Signal();
    return;
  }

  // Make the UserGroup setting persist even if the experiment settings change.
  utils::SetUserGroupPref(profile_->GetPrefs(), utils::GetUserGroup());

  base::flat_set<AppId> launched_before =
      GetLaunchedPreinstalledAppIds(registrar_unsafe());
  utils::SetHasLaunchedAppsBeforePref(profile_->GetPrefs(), launched_before);

  if (utils::GetUserGroup() == UserGroup::kWindow) {
    DCHECK(apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(
        profile_));
    auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile_);
    DCHECK(proxy);
    apps_that_experiment_setup_set_supported_links_ =
        SetSupportedLinksPreferenceForPreinstalledApps(registrar_unsafe(),
                                                       *proxy);
  }

  StartOverridesAndObservations();
}

void PreinstalledWebAppWindowExperiment::StartOverridesAndObservations() {
  DCHECK(utils::GetUserGroup() != UserGroup::kUnknown);
  DCHECK(utils::GetEligibilityPref(profile_->GetPrefs()).value_or(false));

  SetUserDisplayModeOverridesForPreinstalledAppsOnRegistrar(
      registrar_unsafe(), profile_->GetPrefs(), /*notify_all=*/true);

  // Start listening for `OnWebAppUserDisplayModeChanged`.
  registrar_observation_.Observe(&registrar_unsafe());
  DCHECK(
      apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile_));
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile_);
  DCHECK(proxy);
  // Start listening for `OnPreferredAppChanged`.
  preferred_apps_observation_.Observe(&proxy->PreferredAppsList());

  setup_done_for_testing_.Signal();
}

void PreinstalledWebAppWindowExperiment::CleanUp() {
  // Ensure we aren't listening for changes before making changes.
  registrar_observation_.Reset();
  preferred_apps_observation_.Reset();

  if (base::FeatureList::IsEnabled(kWebAppWindowExperimentCleanup)) {
    PersistStateFromPrefsToWebAppDb(profile_->GetPrefs(),
                                    *WebAppProvider::GetForWebApps(profile_));

    utils::DeleteExperimentPrefs(profile_->GetPrefs());
  }

  setup_done_for_testing_.Signal();
}

void PreinstalledWebAppWindowExperiment::OnAppRegistrarDestroyed() {
  registrar_observation_.Reset();
}

void PreinstalledWebAppWindowExperiment::OnWebAppUserDisplayModeChanged(
    const AppId& app_id,
    UserDisplayMode user_display_mode) {
  auto* app = registrar_unsafe().GetAppById(app_id);
  if (!app || !app->IsPreinstalledApp()) {
    return;
  }

  // Avoid recursively notifying ourselves when setting the display mode.
  if (!utils::GetAppIdsWithUserOverridenDisplayModePref(profile_->GetPrefs())
           .contains(app_id)) {
    // Record user setting in prefs and remove override from registrar.
    utils::SetUserOverridenDisplayModePref(profile_->GetPrefs(), app_id);
    SetUserDisplayModeOverridesForPreinstalledAppsOnRegistrar(
        registrar_unsafe(), profile_->GetPrefs(), /*notify_all=*/false);
    // Update observers again now that the override has been removed, so the
    // registrar will return `user_display_mode` if queried directly.
    registrar_unsafe().NotifyWebAppUserDisplayModeChanged(app_id,
                                                          user_display_mode);
    return;
  }

  absl::optional<apps::DefaultAppName> app_name =
      apps::PreinstalledWebAppIdToName(app_id);
  if (!app_name.has_value()) {
    LOG(WARNING) << "Unknown preinstalled app " << app->untranslated_name()
                 << " ID " << app_id;
    return;
  }

  utils::RecordDisplayModeChangeHistogram(profile_->GetPrefs(),
                                          user_display_mode, *app_name);
}

void PreinstalledWebAppWindowExperiment::OnPreferredAppChanged(
    const std::string& app_id,
    bool is_preferred_app) {
  auto* app = registrar_unsafe().GetAppById(app_id);
  if (!app || !app->IsPreinstalledApp()) {
    return;
  }

  // Ignore the first observation for each app that may result from the
  // experiment setup's call to `SetSupportedLinksPreference`.
  // Note: this allows for `SetSupportedLinksPreference` to be async (or not)
  // and for `OnPreferredAppChanged` to be called only for a subset of apps that
  // changed state.
  if (apps_that_experiment_setup_set_supported_links_.erase(app_id) &&
      is_preferred_app) {
    return;
  }

  absl::optional<apps::DefaultAppName> app_name =
      apps::PreinstalledWebAppIdToName(app_id);
  if (!app_name.has_value()) {
    LOG(WARNING) << "Unknown default app " << app->untranslated_name() << " ID "
                 << app_id;
    return;
  }

  utils::RecordLinkCapturingChangeHistogram(profile_->GetPrefs(),
                                            is_preferred_app, *app_name);
}

void PreinstalledWebAppWindowExperiment::OnPreferredAppsListWillBeDestroyed(
    apps::PreferredAppsListHandle* handle) {
  preferred_apps_observation_.Reset();
}

WebAppRegistrar& PreinstalledWebAppWindowExperiment::registrar_unsafe() const {
  return WebAppProvider::GetForWebApps(profile_)->registrar_unsafe();
}

}  //  namespace web_app
