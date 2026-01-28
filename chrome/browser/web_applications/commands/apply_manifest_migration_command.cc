// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/apply_manifest_migration_command.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/commands/command_result.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/jobs/uninstall/remove_install_source_job.h"
#include "chrome/browser/web_applications/jobs/uninstall/uninstall_job.h"
#include "chrome/browser/web_applications/locks/all_apps_lock.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/os_integration/os_integration_sub_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/scheduler/apply_manifest_migration_result.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_filter.h"
#include "chrome/browser/web_applications/web_app_management_type.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "third_party/blink/public/common/features.h"

namespace web_app {

namespace {

// Correctness check to ensure that the destination app passed into this command
// has the source app as one of the validated manifest sources.
bool IsSourceAppInDestinationAppMigrationSources(
    AllAppsLock& all_apps_lock,
    const webapps::AppId& source_id,
    const webapps::AppId& destination_id) {
  webapps::ManifestId source_manifest_id =
      all_apps_lock.registrar().GetAppById(source_id)->manifest_id();
  const WebApp* destination_app =
      all_apps_lock.registrar().GetAppById(destination_id);
  CHECK(destination_app);
  for (const auto& migration_sources :
       destination_app->validated_migration_sources()) {
    CHECK(migration_sources.has_manifest_id());
    if (migration_sources.manifest_id() == source_manifest_id) {
      return true;
    }
  }
  return false;
}

base::Value SynchronizeOptionsAsDebugValue(
    const SynchronizeOsOptions& options) {
  base::DictValue options_value;
  options_value.Set("add_shortcut_to_desktop", options.add_shortcut_to_desktop);
  options_value.Set("add_to_quick_launch_bar", options.add_to_quick_launch_bar);
  options_value.Set("shortcut_creation_reason", base::ToString(options.reason));
  return base::Value(std::move(options_value));
}

}  // namespace

ApplyManifestMigrationCommand::ApplyManifestMigrationCommand(
    const webapps::AppId& source_app_id,
    const webapps::AppId& destination_app_id,
    Profile* profile,
    std::unique_ptr<ScopedKeepAlive> keep_alive,
    std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive,
    ApplyManifestMigrationResultCallback callback)
    : WebAppCommand<AllAppsLock, ApplyManifestMigrationResult>(
          "ApplyManifestMigrationCommand",
          AllAppsLockDescription(),
          base::BindOnce([](ApplyManifestMigrationResult result) {
            base::UmaHistogramEnumeration("WebApp.Migration.ApplyResult",
                                          result);
            return result;
          }).Then(std::move(callback)),
          /*args_for_shutdown=*/ApplyManifestMigrationResult::kSystemShutdown),
      source_app_id_(source_app_id),
      destination_app_id_(destination_app_id),
      profile_(profile),
      keep_alive_(std::move(keep_alive)),
      profile_keep_alive_(std::move(profile_keep_alive)) {
  GetMutableDebugValue().Set("source_app_id", source_app_id_);
  GetMutableDebugValue().Set("destination_app_id", destination_app_id_);
}

ApplyManifestMigrationCommand::~ApplyManifestMigrationCommand() = default;

void ApplyManifestMigrationCommand::StartWithLock(
    std::unique_ptr<AllAppsLock> lock) {
  all_apps_lock_ = std::move(lock);

  CHECK(base::FeatureList::IsEnabled(blink::features::kWebAppMigrationApi));

  // Exit early if the source app cannot be migrated to a different app.
  if (!all_apps_lock_->registrar().AppMatches(
          source_app_id_, WebAppFilter::IsAppValidMigrationSource())) {
    CompleteCommandAndSelfDestruct(
        ApplyManifestMigrationResult::kSourceAppInvalidForMigration);
    return;
  }

  // Exit early if the destination app is not in the registrar or is scheduled
  // to be uninstalled.
  std::optional<proto::InstallState> destination_app_install_state =
      all_apps_lock_->registrar().GetInstallState(destination_app_id_);
  if (!destination_app_install_state.has_value()) {
    CompleteCommandAndSelfDestruct(
        ApplyManifestMigrationResult::kDestinationAppInvalid);
    return;
  }

  GetMutableDebugValue().Set(
      "source_app_install_state",
      base::ToString(
          all_apps_lock_->registrar().GetInstallState(source_app_id_)));
  GetMutableDebugValue().Set("destination_app_install_state",
                             base::ToString(*destination_app_install_state));

  // Destination and source app should be linked, failure of correctness checks
  // should stop the migration command from running.
  if (!IsSourceAppInDestinationAppMigrationSources(
          *all_apps_lock_, source_app_id_, destination_app_id_)) {
    CompleteCommandAndSelfDestruct(
        ApplyManifestMigrationResult::kDestinationAppDoesNotLinkToSourceApp);
    return;
  }

  // Uninstall the source app if the destination app is already installed with
  // OS integration.
  if (destination_app_install_state == proto::INSTALLED_WITH_OS_INTEGRATION) {
    UninstallSourceApp();
    return;
  }

  // Get the shortcut info metadata for the source app to obtain the OS
  // integration locations on the device.
  all_apps_lock_->os_integration_manager().GetShortcutInfoForAppFromRegistrar(
      source_app_id_,
      base::BindOnce(&ApplyManifestMigrationCommand::
                         StartGatheringOsIntegrationInfoForSourceApp,
                     weak_factory_.GetWeakPtr()));
}

void ApplyManifestMigrationCommand::StartGatheringOsIntegrationInfoForSourceApp(
    std::unique_ptr<ShortcutInfo> source_app_shortcut_info) {
  // Recreate full shortcuts if current OS integration information is not found.
  GetMutableDebugValue().Set("shortcut_info_obtained_for_source_app",
                             !!source_app_shortcut_info);
  if (!source_app_shortcut_info) {
    SynchronizeOsOptions os_options{.add_shortcut_to_desktop = true,
                                    .add_to_quick_launch_bar = true,
                                    .reason = SHORTCUT_CREATION_BY_USER};
    SynchronizeOsIntegration(os_options);
    return;
  }

  all_apps_lock_->os_integration_manager().GetAppExistingShortCutLocation(
      base::BindOnce(
          &ApplyManifestMigrationCommand::MigrateOsIntegrationFromSourceApp,
          weak_factory_.GetWeakPtr()),
      std::move(source_app_shortcut_info));
}

void ApplyManifestMigrationCommand::MigrateOsIntegrationFromSourceApp(
    ShortcutLocations source_app_locations) {
  GetMutableDebugValue().Set("shortcut_locations_for_source_app",
                             source_app_locations.ToDebugValue());
  // Platforms like Mac don't fetch the 'run on os login' property from the
  // GetAppExistingShortCutLocation API, so query the registry for that.
  bool run_on_os_login = source_app_locations.in_startup ||
                         all_apps_lock_->registrar()
                                 .GetAppRunOnOsLoginMode(source_app_id_)
                                 .value == RunOnOsLoginMode::kWindowed;
  ValueWithPolicy<RunOnOsLoginMode> destination_rool_allowed =
      all_apps_lock_->registrar().GetAppRunOnOsLoginMode(destination_app_id_);
  {
    ScopedRegistryUpdate update = all_apps_lock_->sync_bridge().BeginUpdate();
    WebApp* destination_app = update->UpdateApp(destination_app_id_);

    // Only allow run on OS login for the destination app if it is allowed by
    // policy.
    if (destination_rool_allowed.user_controllable) {
      destination_app->SetRunOnOsLoginMode(run_on_os_login
                                               ? RunOnOsLoginMode::kWindowed
                                               : RunOnOsLoginMode::kNotRun);
    }
    destination_app->SetInstallState(proto::INSTALLED_WITH_OS_INTEGRATION);
    if (destination_app->GetSources().Has(WebAppManagement::kUserInstalled)) {
      destination_app->AddSource(WebAppManagement::kSync);
    }
  }

  SynchronizeOsOptions os_options{
      .add_shortcut_to_desktop = source_app_locations.on_desktop,
      .add_to_quick_launch_bar = source_app_locations.in_quick_launch_bar,
      .reason = SHORTCUT_CREATION_BY_USER};
  SynchronizeOsIntegration(os_options);
}

void ApplyManifestMigrationCommand::SynchronizeOsIntegration(
    SynchronizeOsOptions os_options) {
  GetMutableDebugValue().Set("synchronize_debug_value",
                             SynchronizeOptionsAsDebugValue(os_options));
  all_apps_lock_->os_integration_manager().Synchronize(
      destination_app_id_,
      base::BindOnce(&ApplyManifestMigrationCommand::UninstallSourceApp,
                     weak_factory_.GetWeakPtr()),
      os_options);
}

void ApplyManifestMigrationCommand::UninstallSourceApp() {
  GetMutableDebugValue().Set("os_integration_set", true);
  const WebApp* source_app =
      all_apps_lock_->registrar().GetAppById(source_app_id_);
  CHECK(source_app);

  // Ensure that all sources that can be removed from the app are user
  // uninstallable.
  CHECK_EQ(kUserUninstallableSources,
           base::Union(kUserUninstallableSources, source_app->GetSources()))
      << "Source app has sources that are not user uninstallable";
  remove_source_app_job_ = std::make_unique<RemoveInstallSourceJob>(
      webapps::WebappUninstallSource::kAppMigration, *profile_,
      *GetMutableDebugValue().EnsureDict("RemoveInstallSourceJob"),
      source_app_id_, source_app->GetSources());
  remove_source_app_job_->Start(
      *all_apps_lock_,
      base::BindOnce(
          &ApplyManifestMigrationCommand::AppUninstalledCompleteMigration,
          weak_factory_.GetWeakPtr()));
}

void ApplyManifestMigrationCommand::AppUninstalledCompleteMigration(
    webapps::UninstallResultCode uninstall_code) {
  GetMutableDebugValue().Set("source_app_uninstall_code",
                             base::ToString(uninstall_code));
  if (!webapps::UninstallSucceeded(uninstall_code)) {
    CompleteCommandAndSelfDestruct(
        ApplyManifestMigrationResult::kUnableToRemoveSourceApp);
    return;
  }

  // TODO(crbug.com/465762477): Update sync data here for web apps to point to
  // source app once implemented.
  CompleteCommandAndSelfDestruct(
      ApplyManifestMigrationResult::kAppMigrationAppliedSuccessfully);
}

void ApplyManifestMigrationCommand::CompleteCommandAndSelfDestruct(
    ApplyManifestMigrationResult result) {
  GetMutableDebugValue().Set("migration_result", base::ToString(result));
  CompleteAndSelfDestruct(CommandResult::kSuccess, result);
}

}  // namespace web_app
