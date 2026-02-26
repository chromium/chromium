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
#include "base/types/pass_key.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/commands/command_result.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/jobs/uninstall/remove_install_source_job.h"
#include "chrome/browser/web_applications/jobs/uninstall/uninstall_job.h"
#include "chrome/browser/web_applications/locks/all_apps_lock.h"
#include "chrome/browser/web_applications/model/migration_behavior.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/os_integration/os_integration_sub_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"
#include "chrome/browser/web_applications/proto/web_app.pb.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/scheduler/apply_manifest_migration_result.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_filter.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_management_type.h"
#include "chrome/browser/web_applications/web_app_proto_utils.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/sync/protocol/web_app_specifics.pb.h"
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
    MigrationBehavior migration_behavior,
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
      migration_behavior_(migration_behavior),
      profile_(profile),
      keep_alive_(std::move(keep_alive)),
      profile_keep_alive_(std::move(profile_keep_alive)) {
  GetMutableDebugValue().Set("source_app_id", source_app_id_);
  GetMutableDebugValue().Set("destination_app_id", destination_app_id_);
  GetMutableDebugValue().Set("migration_behavior",
                             base::ToString(migration_behavior_));
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

  bool is_forced_migration = (migration_behavior_ == MigrationBehavior::kForce);

  // Handle all the use-cases where the forced migration isn't necessary.
  if (!is_forced_migration) {
    // Uninstall the source app if the destination app is already installed with
    // OS integration.
    if (destination_app_install_state == proto::INSTALLED_WITH_OS_INTEGRATION) {
      SetupDestinationAppUninstallSourceApp();
      return;
    }

    // For migrations that are not forced, start collecting OS integration
    // state. The app and icon does not need to match the source app in this
    // case.
    gather_migration_source_info_job_ =
        std::make_unique<GatherMigrationSourceInfoJob>(
            *all_apps_lock_, source_app_id_, destination_app_id_,
            base::BindOnce(
                &ApplyManifestMigrationCommand::OnMigrationSourceInfoGathered,
                weak_factory_.GetWeakPtr()));
    gather_migration_source_info_job_->Start();
    return;
  }

  // For forced migrations, set up name, icon metadata and the sync fields to
  // mimic the source app. Set up end state on disk as well.
  CHECK(is_forced_migration);
  const WebApp* source_app =
      all_apps_lock_->registrar().GetAppById(source_app_id_);
  {
    ScopedRegistryUpdate update = all_apps_lock_->sync_bridge().BeginUpdate();
    WebApp* destination_app = update->UpdateApp(destination_app_id_);
    destination_app->SetName(source_app->untranslated_name());
    destination_app->SetManifestIcons(source_app->manifest_icons());
    destination_app->SetTrustedIcons(source_app->trusted_icons());
    destination_app->SetDownloadedIconSizes(
        IconPurpose::ANY, source_app->downloaded_icon_sizes(IconPurpose::ANY));
    destination_app->SetDownloadedIconSizes(
        IconPurpose::MASKABLE,
        source_app->downloaded_icon_sizes(IconPurpose::MASKABLE));
    destination_app->SetDownloadedIconSizes(
        IconPurpose::MONOCHROME,
        source_app->downloaded_icon_sizes(IconPurpose::MONOCHROME));

    // There are no monochrome trusted icons.
    destination_app->SetStoredTrustedIconSizes(
        IconPurpose::ANY,
        source_app->stored_trusted_icon_sizes(IconPurpose::ANY));
    destination_app->SetStoredTrustedIconSizes(
        IconPurpose::MASKABLE,
        source_app->stored_trusted_icon_sizes(IconPurpose::MASKABLE));
  }

  all_apps_lock_->icon_manager().CopyIconsFromOneAppToAnother(
      source_app_id_, destination_app_id_,
      base::PassKey<ApplyManifestMigrationCommand>(),
      base::BindOnce(&ApplyManifestMigrationCommand::OnIconsCopied,
                     weak_factory_.GetWeakPtr()));
}

void ApplyManifestMigrationCommand::OnIconsCopied(bool success) {
  GetMutableDebugValue().Set("icon_copy_successful_for_forced_migration",
                             success);

  all_apps_lock_->install_manager().NotifyWebAppManifestUpdated(
      destination_app_id_);

  if (!success) {
    CompleteCommandAndSelfDestruct(
        ApplyManifestMigrationResult::kAppMigrationFailedDuringIconCopy);
    return;
  }

  gather_migration_source_info_job_ =
      std::make_unique<GatherMigrationSourceInfoJob>(
          *all_apps_lock_, source_app_id_, destination_app_id_,
          base::BindOnce(
              &ApplyManifestMigrationCommand::OnMigrationSourceInfoGathered,
              weak_factory_.GetWeakPtr()));
  gather_migration_source_info_job_->Start();
}

void ApplyManifestMigrationCommand::OnMigrationSourceInfoGathered(
    std::optional<GatherMigrationSourceInfoJobResult> migration_state) {
  if (migration_state) {
    GetMutableDebugValue().Set("migration_source_info",
                               migration_state->ToDebugValue());
  }

  // Recreate full shortcuts if current OS integration information is not found.
  GetMutableDebugValue().Set("shortcut_info_obtained_for_source_app",
                             migration_state.has_value());
  if (!migration_state) {
    SynchronizeOsOptions os_options{.add_shortcut_to_desktop = true,
                                    .add_to_quick_launch_bar = true,
                                    .reason = SHORTCUT_CREATION_BY_USER};
    SynchronizeOsIntegration(os_options);
    return;
  }

  {
    ScopedRegistryUpdate update = all_apps_lock_->sync_bridge().BeginUpdate();
    WebApp* destination_app = update->UpdateApp(destination_app_id_);

    if (migration_state->run_on_os_login_mode != RunOnOsLoginMode::kNotRun) {
      destination_app->SetRunOnOsLoginMode(
          migration_state->run_on_os_login_mode);
    }
    destination_app->SetInstallState(proto::INSTALLED_WITH_OS_INTEGRATION);
    destination_app->SetUserDisplayMode(migration_state->user_display_mode);
  }

  SynchronizeOsOptions os_options{
      .add_shortcut_to_desktop = migration_state->shortcut_locations.on_desktop,
      .add_to_quick_launch_bar =
          migration_state->shortcut_locations.in_quick_launch_bar,
      .reason = SHORTCUT_CREATION_BY_USER};
  SynchronizeOsIntegration(os_options);
}

void ApplyManifestMigrationCommand::SynchronizeOsIntegration(
    SynchronizeOsOptions os_options) {
  GetMutableDebugValue().Set("synchronize_debug_value",
                             SynchronizeOptionsAsDebugValue(os_options));
  all_apps_lock_->os_integration_manager().Synchronize(
      destination_app_id_,
      base::BindOnce(
          &ApplyManifestMigrationCommand::SetupDestinationAppUninstallSourceApp,
          weak_factory_.GetWeakPtr()),
      os_options);
}

void ApplyManifestMigrationCommand::SetupDestinationAppUninstallSourceApp() {
  // Set the destination app to be have all information necessary for syncing.
  const webapps::ManifestId& source_manifest_id =
      all_apps_lock_->registrar().GetComputedManifestId(source_app_id_);
  CHECK(source_manifest_id.is_valid());
  {
    ScopedRegistryUpdate update = all_apps_lock_->sync_bridge().BeginUpdate();
    WebApp* destination_app = update->UpdateApp(destination_app_id_);
    CHECK(destination_app);
    if (destination_app->GetSources().Has(WebAppManagement::kUserInstalled)) {
      destination_app->AddSource(WebAppManagement::kSync);
    }

    // Set the source app's manifest id to be synced.
    destination_app->SetMigratedFromManifestIdInSyncProto(source_manifest_id);
  }
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
    // Note: We could still launch the migrated-to app. Avoiding for simplicity
    // - if this metric happens frequently, we can consider allowing the
    // launch too.
    CompleteCommandAndSelfDestruct(
        ApplyManifestMigrationResult::kUnableToRemoveSourceApp);
    return;
  }

  apps::AppLaunchParams params(destination_app_id_,
                               // Note: This is overridden with the web app
                               // config, as per kOverrideWithWebAppConfig.
                               apps::LaunchContainer::kLaunchContainerWindow,
                               // Note: This is overridden with the web app
                               // config, as per kOverrideWithWebAppConfig.
                               WindowOpenDisposition::NEW_WINDOW,
                               apps::LaunchSource::kFromMigration);
  LaunchWebAppWindowSetting launch_setting =
      LaunchWebAppWindowSetting::kOverrideWithWebAppConfig;

  all_apps_lock_->ui_manager().LaunchWebApp(
      std::move(params), launch_setting, *profile_,
      base::BindOnce(&ApplyManifestMigrationCommand::OnAppLaunched,
                     weak_factory_.GetWeakPtr()),
      *all_apps_lock_);
}

void ApplyManifestMigrationCommand::OnAppLaunched(
    base::WeakPtr<Browser> browser,
    base::WeakPtr<content::WebContents> web_contents,
    apps::LaunchContainer container,
    base::Value debug_value) {
  GetMutableDebugValue().Set("launch_web_app_debug_value",
                             std::move(debug_value));
  if (!browser || !web_contents) {
    CompleteCommandAndSelfDestruct(
        ApplyManifestMigrationResult::
            kAppMigrationAppliedSuccessfullyLaunchFailed);
    return;
  }
  CompleteCommandAndSelfDestruct(
      ApplyManifestMigrationResult::kAppMigrationAppliedSuccessfully);
}

void ApplyManifestMigrationCommand::CompleteCommandAndSelfDestruct(
    ApplyManifestMigrationResult result) {
  GetMutableDebugValue().Set("migration_result", base::ToString(result));
  switch (result) {
    case ApplyManifestMigrationResult::kAppMigrationAppliedSuccessfully:
    case ApplyManifestMigrationResult::
        kAppMigrationAppliedSuccessfullyLaunchFailed:
      all_apps_lock_->install_manager().NotifyWebAppMigrated(
          source_app_id_, destination_app_id_);
      break;
    case ApplyManifestMigrationResult::kSourceAppInvalidForMigration:
    case ApplyManifestMigrationResult::kDestinationAppInvalid:
    case ApplyManifestMigrationResult::kDestinationAppDoesNotLinkToSourceApp:
    case ApplyManifestMigrationResult::kAppMigrationFailedDuringIconCopy:
    case ApplyManifestMigrationResult::kUnableToRemoveSourceApp:
      break;
    case ApplyManifestMigrationResult::kSystemShutdown:
      NOTREACHED();
  }
  CompleteAndSelfDestruct(CommandResult::kSuccess, result);
}

}  // namespace web_app
