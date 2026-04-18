// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/manifest_update_manager.h"

#include <optional>

#include "base/containers/map_util.h"
#include "base/memory/weak_ptr.h"
#include "base/types/optional_util.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/scheduler/manifest_silent_update_result.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_filter.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_management_type.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/webapps/browser/features.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-shared.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/web_applications/web_app_system_web_app_delegate_map_utils.h"
#endif

class Profile;

namespace web_app {

ManifestUpdateManager::ManifestUpdateManager() = default;

ManifestUpdateManager::~ManifestUpdateManager() = default;

#if BUILDFLAG(IS_CHROMEOS)
void ManifestUpdateManager::SetSystemWebAppDelegateMap(
    const ash::SystemWebAppDelegateMap* system_web_apps_delegate_map) {
  system_web_apps_delegate_map_ = system_web_apps_delegate_map;
}
#endif

void ManifestUpdateManager::SetProvider(base::PassKey<WebAppProvider>,
                                        WebAppProvider& provider) {
  provider_ = &provider;
}

void ManifestUpdateManager::Start() {
  install_manager_observation_.Observe(&provider_->install_manager());
  CHECK(!started_);
  started_ = true;
}

void ManifestUpdateManager::Shutdown() {
  install_manager_observation_.Reset();
  started_ = false;
}

void ManifestUpdateManager::OnManifestSeenOnPrimaryPage(
    content::WebContents& web_contents,
    const blink::mojom::ManifestPtr& manifest,
    base::PassKey<WebAppTabHelper>) {
  // Developer-specified manifests should always have a valid manifest URL.
  CHECK(manifest->manifest_url.is_valid());
  if (!started_) {
    return;
  }


  webapps::AppId app_id = GenerateAppIdFromManifest(*manifest);

  if (provider_->registrar_unsafe().AppMatches(app_id,
                                               WebAppFilter::IsIsolatedApp())) {
    return;
  }

  if (provider_->registrar_unsafe().AppMatches(
          app_id, WebAppFilter::IsIsolatedSubApp())) {
    TriggerManifestUpdateProcess(web_contents, app_id);
    return;
  }

  if (base::FeatureList::IsEnabled(blink::features::kWebAppMigrationApi)) {
    if (!manifest->migrate_from.empty()) {
      provider_->scheduler().ScheduleWebAppInstallFromMigrateFromField(
          web_contents.GetWeakPtr(), manifest.Clone(), base::DoNothing());

      WebAppTabHelper* tab_helper =
          WebAppTabHelper::FromWebContents(&web_contents);
      if (tab_helper && tab_helper->window_app_id().has_value()) {
        const webapps::AppId& window_app_id = *tab_helper->window_app_id();
        for (const auto& migrate_from : manifest->migrate_from) {
          if (GenerateAppIdFromManifestId(migrate_from->id) == window_app_id &&
              migrate_from->install_url.has_value()) {
            std::optional<base::Time> previous_time_for_silent_icon_update =
                base::OptionalFromPtr(base::FindOrNull(
                    update_check_for_silent_updates_, window_app_id));
            provider_->scheduler().FetchManifestAndUpdate(
                *migrate_from->install_url, migrate_from->id,
                previous_time_for_silent_icon_update,
                /*force_trusted_silent_update=*/false,
                base::BindOnce(&ManifestUpdateManager::
                                   OnMigrationFetchManifestAndUpdateComplete,
                               weak_factory_.GetWeakPtr(), window_app_id));
          }
        }
      }
    }
    if (manifest->migrate_to &&
        provider_->registrar_unsafe().AppMatches(
            GenerateAppIdFromManifest(*manifest),
            WebAppFilter::CanAppInstallTargetMigrationApp())) {
      provider_->scheduler().ScheduleInstallMigrateToApp(
          manifest->id, manifest->migrate_to->id,
          manifest->migrate_to->install_url, base::DoNothing());
    }
  }

  TriggerManifestUpdateProcess(web_contents, app_id);
}

void ManifestUpdateManager::TriggerManifestUpdateProcess(
    content::WebContents& web_contents,
    const webapps::AppId& app_id) {
  std::optional<base::Time> previous_time_for_silent_icon_update =
      base::OptionalFromPtr(
          base::FindOrNull(update_check_for_silent_updates_, app_id));

  provider_->scheduler().ScheduleManifestSilentUpdate(
      web_contents, previous_time_for_silent_icon_update,
      base::BindOnce(&ManifestUpdateManager::OnManifestSilentUpdateComplete,
                     weak_factory_.GetWeakPtr(), web_contents.GetWeakPtr(),
                     app_id));
}

void ManifestUpdateManager::OnManifestSilentUpdateComplete(
    base::WeakPtr<content::WebContents> contents,
    const webapps::AppId& app_id,
    ManifestSilentUpdateCompletionInfo completion_info) {
  bool any_update_occurred;
  switch (completion_info.result) {
    case ManifestSilentUpdateCheckResult::kAppUpdateFailedDuringInstall:
    case ManifestSilentUpdateCheckResult::kSystemShutdown:
    case ManifestSilentUpdateCheckResult::kAppUpToDate:
    case ManifestSilentUpdateCheckResult::kIconReadFromDiskFailed:
    case ManifestSilentUpdateCheckResult::kWebContentsWasDestroyed:
    case ManifestSilentUpdateCheckResult::kPendingIconWriteToDiskFailed:
    case ManifestSilentUpdateCheckResult::kInvalidManifest:
    case ManifestSilentUpdateCheckResult::kInvalidPendingUpdateInfo:
    case ManifestSilentUpdateCheckResult::kUserNavigated:
    case ManifestSilentUpdateCheckResult::kManifestToWebAppInstallInfoError:
    case ManifestSilentUpdateCheckResult::kAppNotAllowedToUpdate:
      any_update_occurred = false;
      break;
    case ManifestSilentUpdateCheckResult::kAppOnlyHasSecurityUpdate:
    case ManifestSilentUpdateCheckResult::kAppSilentlyUpdated:
    case ManifestSilentUpdateCheckResult::kAppHasNonSecurityAndSecurityChanges:
    case ManifestSilentUpdateCheckResult::kAppHasSecurityUpdateDueToThrottle:
    case ManifestSilentUpdateCheckResult::
        kAppSilentlyUpdatedDueToSmallIconComparison:
      any_update_occurred = true;
      break;
  }

  // If a manifest update happened successfully, record feature usage of
  // applying a manifest.
  if (any_update_occurred && contents) {
    page_load_metrics::MetricsWebContentsObserver::RecordFeatureUsage(
        contents->GetPrimaryMainFrame(),
        blink::mojom::WebFeature::kWebAppManifestUpdate);
  }

  // Track time for throttling future silent icon updates if the current update
  // triggered a silent icon update.
  if (completion_info.time_for_icon_diff_check.has_value()) {
    update_check_for_silent_updates_[app_id] =
        *completion_info.time_for_icon_diff_check;
  }
}

void ManifestUpdateManager::OnMigrationFetchManifestAndUpdateComplete(
    const webapps::AppId& app_id,
    FetchManifestAndUpdateCompletionInfo completion_info) {
  if (completion_info.time_for_icon_diff_check.has_value()) {
    update_check_for_silent_updates_[app_id] =
        *completion_info.time_for_icon_diff_check;
  }
}

// WebAppInstallManager:
void ManifestUpdateManager::OnWebAppWillBeUninstalled(
    const webapps::AppId& app_id) {
  CHECK(started_);

  // Clear any data necessary for throttling updates for the current web app.
  update_check_for_silent_updates_.erase(app_id);
}

void ManifestUpdateManager::OnWebAppInstallManagerDestroyed() {
  install_manager_observation_.Reset();
}

}  // namespace web_app
