// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/jobs/finalize_install_job.h"

#include <map>
#include <utility>

#include "ash/constants/web_app_id_constants.h"
#include "base/barrier_callback.h"
#include "base/check_is_test.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/types/optional_ref.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/commands/web_app_uninstall_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_integrity_block_data.h"
#include "chrome/browser/web_applications/jobs/finalize_install_job.h"
#include "chrome/browser/web_applications/jobs/uninstall/remove_install_source_job.h"
#include "chrome/browser/web_applications/jobs/uninstall/remove_install_url_job.h"
#include "chrome/browser/web_applications/jobs/uninstall/remove_web_app_job.h"
#include "chrome/browser/web_applications/model/app_installed_by.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcuts_menu.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/proto/web_app.equal.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/scope_extension_info.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_generator.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_management_type.h"
#include "chrome/browser/web_applications/web_app_origin_association_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_scope.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_translation_manager.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/sync/base/time.h"
#include "components/sync/protocol/web_app_specifics.pb.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "components/webapps/common/web_app_id.h"
#include "components/webapps/isolated_web_apps/types/iwa_version.h"
#include "components/webapps/isolated_web_apps/types/storage_location.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/origin.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/ash/experiences/system_web_apps/types/system_web_app_data.h"
#endif

namespace web_app {

// Overwrite the user display mode if the install source indicates a
// user-initiated installation
bool ShouldInstallOverwriteUserDisplayMode(
    webapps::WebappInstallSource source) {
  using InstallSource = webapps::WebappInstallSource;
  switch (source) {
    case InstallSource::MENU_BROWSER_TAB:
    case InstallSource::MENU_CUSTOM_TAB:
    case InstallSource::AUTOMATIC_PROMPT_BROWSER_TAB:
    case InstallSource::AUTOMATIC_PROMPT_CUSTOM_TAB:
    case InstallSource::API_BROWSER_TAB:
    case InstallSource::API_CUSTOM_TAB:
    case InstallSource::AMBIENT_BADGE_BROWSER_TAB:
    case InstallSource::AMBIENT_BADGE_CUSTOM_TAB:
    case InstallSource::RICH_INSTALL_UI_WEBLAYER:
    case InstallSource::ARC:
    case InstallSource::CHROME_SERVICE:
    case InstallSource::ML_PROMOTION:
    case InstallSource::OMNIBOX_INSTALL_ICON:
    case InstallSource::MENU_CREATE_SHORTCUT:
    case InstallSource::PROFILE_MENU:
    case InstallSource::ALMANAC_INSTALL_APP_URI:
    case InstallSource::WEBAPK_RESTORE:
    case InstallSource::OOBE_APP_RECOMMENDATIONS:
    case InstallSource::WEB_INSTALL:
    case InstallSource::CHROMEOS_HELP_APP:
      return true;
    case InstallSource::DEVTOOLS:
    case InstallSource::MANAGEMENT_API:
    case InstallSource::INTERNAL_DEFAULT:
    case InstallSource::IWA_DEV_UI:
    case InstallSource::IWA_DEV_COMMAND_LINE:
    case InstallSource::IWA_GRAPHICAL_INSTALLER:
    case InstallSource::IWA_EXTERNAL_POLICY:
    case InstallSource::IWA_SHIMLESS_RMA:
    case InstallSource::EXTERNAL_DEFAULT:
    case InstallSource::EXTERNAL_POLICY:
    case InstallSource::EXTERNAL_LOCK_SCREEN:
    case InstallSource::SYSTEM_DEFAULT:
    case InstallSource::SYNC:
    case InstallSource::SUB_APP:
    case InstallSource::KIOSK:
    case InstallSource::PRELOADED_OEM:
    case InstallSource::PRELOADED_DEFAULT:
    case InstallSource::MICROSOFT_365_SETUP:
    case InstallSource::MIGRATION:
      return false;
  }
}

FinalizeJobOptions::IwaOptions::IwaOptions(
    IsolatedWebAppStorageLocation location,
    std::optional<IsolatedWebAppIntegrityBlockData> integrity_block_data)
    : location(std::move(location)),
      integrity_block_data(std::move(integrity_block_data)) {}

FinalizeJobOptions::IwaOptions::~IwaOptions() = default;

FinalizeJobOptions::IwaOptions::IwaOptions(const IwaOptions&) = default;

FinalizeJobOptions::FinalizeJobOptions(
    webapps::WebappInstallSource install_surface)
    : source(ConvertInstallSurfaceToWebAppSource(install_surface)),
      install_surface(install_surface) {}

FinalizeJobOptions::~FinalizeJobOptions() = default;

FinalizeJobOptions::FinalizeJobOptions(const FinalizeJobOptions&) = default;

std::optional<ApiApprovalState> AdjustFileHandlerUserApproval(
    const WebAppRegistrar& registrar,
    base::optional_ref<const WebApp> existing_app,
    const WebApp& new_app) {
  // Reset the previously obtained file handler approval if the new app's file
  // handlers form a superset of the existing (approved) file handlers.
  // Force-installed IWAs are exempt from this rule.
  if (existing_app &&
      existing_app->file_handler_approval_state() ==
          ApiApprovalState::kAllowed &&
      !registrar.AppMatches(new_app.app_id(),
                            WebAppFilter::PolicyInstalledIsolatedWebApp()) &&
      !AreNewFileHandlersASubsetOfOld(existing_app->file_handlers(),
                                      new_app.file_handlers())) {
    return ApiApprovalState::kRequiresPrompt;
  }

  // Isolated sub-apps inherit the force installed parent app's file handler
  // approval.
  if (new_app.parent_app_id() &&
      registrar.AppMatches(*new_app.parent_app_id(),
                           WebAppFilter::PolicyInstalledIsolatedWebApp())) {
    return registrar.GetAppFileHandlerUserApprovalState(
        *new_app.parent_app_id());
  }

  // First-time IWA installations gain a file handler approval; the user can
  // reset it in settings.
  if (!existing_app &&
      new_app.GetSources().Has(WebAppManagement::Type::kIwaPolicy)) {
    return ApiApprovalState::kAllowed;
  }

  return std::nullopt;
}

#if BUILDFLAG(IS_CHROMEOS)
// When web apps are added to sync on ChromeOS the value of
// user_display_mode_default should be set in certain cases to avoid poor sync
// install states on pre-M122 devices and non-CrOS devices with particular web
// apps.
// See switch for specific cases being mitigated against.
// See go/udm-desync#bookmark=id.cg753kjyrruo for design doc.
// TODO(crbug.com/320771282): Add automated tests.
void ApplyUserDisplayModeSyncMitigations(const FinalizeJobOptions& options,
                                         WebApp& web_app) {
  if (FinalizeInstallJob::DisableUserDisplayModeSyncMitigationsForTesting()) {
    return;
  }

  // Guaranteed by EnsureAppsHaveUserDisplayModeForCurrentPlatform().
  CHECK(web_app.sync_proto().has_user_display_mode_cros());

  // Don't mitigate installations from sync, this is only for installs that will
  // be newly uploaded to sync.
  if (options.install_surface == webapps::WebappInstallSource::SYNC) {
    return;
  }

  // Only mitigate if web app is being added to sync.
  if (options.source != WebAppManagement::Type::kSync) {
    return;
  }

  // Don't override existing default-platform value.
  if (web_app.sync_proto().has_user_display_mode_default()) {
    return;
  }

  sync_pb::WebAppSpecifics sync_proto = web_app.sync_proto();

  switch (web_app.sync_proto().user_display_mode_cros()) {
    case sync_pb::WebAppSpecifics_UserDisplayMode_BROWSER:
      // Pre-M122 CrOS devices use the user_display_mode_default sync field
      // instead of user_display_mode_cros. If user_display_mode_default is ever
      // unset they will fallback to using kStandalone even if
      // user_display_mode_cros is set to kBrowser. This mitigation ensures
      // user_display_mode_default is set to kBrowser for these devices. Example
      // user journey:
      // - Install web app as browser shortcut on post-M122 CrOS device.
      // - Sync installation to pre-M122 CrOS device.
      // - Check that it is synced as a browser shortcut.
      // TODO(crbug.com/321617981): Remove when there are sufficiently few
      // pre-M122 CrOS devices in circulation.
      web_app.UpdateDefaultUserDisplayModeInSyncProto(
          sync_pb::WebAppSpecifics_UserDisplayMode_BROWSER);
      break;

    case sync_pb::WebAppSpecifics_UserDisplayMode_STANDALONE: {
      // Ensure standalone averse apps don't get defaulted to kStandalone on
      // non-CrOS devices via sync.
      // Example user journey:
      // - Install Google Docs as a standalone web app.
      // - Sync installation to non-CrOS device.
      // - Check that it is synced as a browser shortcut.
      // TODO(crbug.com/321617972): Remove when Windows/Mac/Linux support for
      // tabbed web apps is in sufficient circulation.
      bool is_standalone_averse_app =
          web_app.app_id() == ash::kGoogleDocsAppId ||
          web_app.app_id() == ash::kGoogleSheetsAppId ||
          web_app.app_id() == ash::kGoogleSlidesAppId;
      if (!is_standalone_averse_app) {
        break;
      }
      web_app.UpdateDefaultUserDisplayModeInSyncProto(
          sync_pb::WebAppSpecifics_UserDisplayMode_BROWSER);
      break;
    }

    case sync_pb::WebAppSpecifics_UserDisplayMode_TABBED:
      // This can only be reached when kDesktopPWAsTabStripSettings is enabled,
      // this is only for testing and is planned to be removed.
      return;
    case sync_pb::WebAppSpecifics_UserDisplayMode_UNSPECIFIED:
      // Ignore unknown UserDisplayMode values.
      return;
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS)

FinalizeInstallJob::FinalizeInstallJob(Profile& profile,
                                       Lock* lock,
                                       WithAppResources* lock_resources,
                                       const WebAppInstallInfo& web_app_info,
                                       const FinalizeJobOptions& options)
    : profile_(profile),
      provider_(WebAppProvider::GetForWebApps(&profile_.get())),
      clock_(&provider_->clock()),
      lock_(lock),
      resources_lock_(lock_resources),
      web_app_info_(web_app_info.Clone()),
      options_(options) {}

FinalizeInstallJob::~FinalizeInstallJob() = default;

void FinalizeInstallJob::Start(InstallFinalizedCallback callback) {
  if (options_.install_state == proto::InstallState::SUGGESTED_FROM_MIGRATION &&
      web_app_info_.migration_sources.empty()) {
    std::move(callback).Run(
        webapps::AppId(), webapps::InstallResultCode::kNoValidMigrationSource);
    return;
  }
  callback_ = std::move(callback);
  webapps::ManifestId manifest_id = web_app_info_.manifest_id();

  bool needs_scope_validation =
      !web_app_info_.scope_extensions.empty() &&
      !web_app_info_.validated_scope_extensions.has_value();
  bool needs_migration_validation =
      base::FeatureList::IsEnabled(blink::features::kWebAppMigrationApi) &&
      !web_app_info_.migration_sources.empty();

  if (options_.skip_origin_association_validation ||
      (!needs_scope_validation && !needs_migration_validation)) {
    OnOriginAssociationValidated(OriginAssociations());
    return;
  }
  OriginAssociations origin_associations;
  if (needs_scope_validation) {
    origin_associations.scope_extensions = web_app_info_.scope_extensions;
  }
  if (needs_migration_validation) {
    origin_associations.migration_sources = web_app_info_.migration_sources;
  }
  origin_association_manager().GetWebAppOriginAssociations(
      manifest_id, std::move(origin_associations),
      base::BindOnce(&FinalizeInstallJob::OnOriginAssociationValidated,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FinalizeInstallJob::OnOriginAssociationValidated(
    OriginAssociations validated_origin_associations) {
  webapps::AppId app_id =
      GenerateAppIdFromManifestId(web_app_info_.manifest_id());

  const WebApp* existing_web_app = registrar().GetAppById(app_id);
  std::unique_ptr<WebApp> web_app;
  if (existing_web_app) {
    web_app = std::make_unique<WebApp>(*existing_web_app);
  } else {
    // TODO(crbug.com/344718166): Ensure that manifest_id corresponds to app_id
    // here.
    web_app = std::make_unique<WebApp>(
        web_app_info_.manifest_id(), web_app_info_.start_url(),
        web_app_info_.scope, web_app_info_.parent_app_id);
    web_app->SetInstallState(proto::SUGGESTED_FROM_ANOTHER_DEVICE);
  }

  ScopeExtensions validated_scope_extensions =
      web_app_info_.validated_scope_extensions.value_or(
          validated_origin_associations.scope_extensions);
  for (auto& scope_extension : validated_scope_extensions) {
    // This is done to prune any queries or fragments from the scope URL which
    // may have been skipped by WebAppOriginAssociationManager validation.
    scope_extension = ScopeExtensionInfo::CreateForScope(
        scope_extension.scope, scope_extension.has_origin_wildcard);
  }
  web_app->SetValidatedScopeExtensions(validated_scope_extensions);
  web_app->SetValidatedMigrationSources(
      validated_origin_associations.migration_sources);

  // When testing, the database state is compared with the in-memory registry,
  // and because proto time has less granularity, this comparison fails unless
  // we pre-downgrade to proto time and back before saving in our database.
  const base::Time now_time =
      syncer::ProtoTimeToTime(syncer::TimeToProtoTime(clock_->Now()));

  // The UI may initiate a full install to overwrite the existing
  // non-locally-installed app. Therefore, `install_state` can be
  // promoted to `INSTALLED_WITH_OS_INTEGRATION`, but not vice versa.
  if (options_.install_state ==
      proto::InstallState::INSTALLED_WITH_OS_INTEGRATION) {
    web_app->SetInstallState(
        proto::InstallState::INSTALLED_WITH_OS_INTEGRATION);
    // The last install time is always updated if the app has been locally
    // installed, but the first install time is updated only once.
    if (web_app->first_install_time().is_null()) {
      web_app->SetFirstInstallTime(now_time);
    }
    // The last install time is updated whenever we (re)install/update.
    web_app->SetLatestInstallTime(now_time);
  }

  // Handle going from SUGGESTED_FROM_ANOTHER_DEVICE ->
  // INSTALLED_WITHOUT_OS_INTEGRATION
  if (web_app->install_state() ==
          proto::InstallState::SUGGESTED_FROM_ANOTHER_DEVICE &&
      options_.install_state ==
          proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION) {
    web_app->SetInstallState(
        proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION);
  }

  // Handle going from SUGGESTED_FROM_MIGRATION to any other state. This
  // ensures that the apps under migration can have their state overridden by
  // flows that are allowed to do so, like a sync install.
  if (web_app->install_state() ==
          proto::InstallState::SUGGESTED_FROM_MIGRATION &&
      options_.install_state != proto::InstallState::SUGGESTED_FROM_MIGRATION) {
    web_app->SetInstallState(options_.install_state);
  }

  // If the app install state is explicitly set to be suggested from migration,
  // honor that over any existing values.
  if (options_.install_state == proto::InstallState::SUGGESTED_FROM_MIGRATION) {
    web_app->SetInstallState(proto::InstallState::SUGGESTED_FROM_MIGRATION);
  }

  // Set |user_display_mode| and any user-controllable fields here if this
  // install is user initiated or it's a new app.
  if (ShouldInstallOverwriteUserDisplayMode(options_.install_surface) ||
      !existing_web_app) {
    DCHECK(web_app_info_.user_display_mode.has_value());
    web_app->SetUserDisplayMode(*web_app_info_.user_display_mode);
  }
  if (options_.run_on_os_login_mode.has_value()) {
    web_app->SetRunOnOsLoginMode(options_.run_on_os_login_mode.value());
  }
#if BUILDFLAG(IS_CHROMEOS)
  ApplyUserDisplayModeSyncMitigations(options_, *web_app);
#endif  // BUILDFLAG(IS_CHROMEOS)
  CHECK(HasCurrentPlatformUserDisplayMode(web_app->sync_proto()));

#if BUILDFLAG(IS_MAC)
  // Only set this flag for newly installed DIY apps on Mac
  if (web_app->is_diy_app() &&
      (!existing_web_app || options_.overwrite_existing_manifest_fields)) {
    web_app->SetDiyAppIconsMaskedOnMac(true);
  }
#endif

  // `WebApp::chromeos_data` has a default value already. Only override if the
  // caller provided a new value.
  if (options_.chromeos_data.has_value()) {
    web_app->SetWebAppChromeOsData(options_.chromeos_data.value());
  }

  if (provider_->policy_manager().IsWebAppInDisabledList(app_id) &&
      web_app->chromeos_data().has_value() &&
      !web_app->chromeos_data()->is_disabled) {
    std::optional<WebAppChromeOsData> cros_data = web_app->chromeos_data();
    cros_data->is_disabled = true;
    web_app->SetWebAppChromeOsData(std::move(cros_data));
  }

#if BUILDFLAG(IS_CHROMEOS)
  // `WebApp::system_web_app_data` has a default value already. Only override if
  // the caller provided a new value.
  if (options_.system_web_app_data.has_value()) {
    web_app->client_data()->system_web_app_data =
        options_.system_web_app_data.value();
  }
#endif

  if (options_.iwa_options) {
    UpdateIsolationDataAndResetPendingUpdateInfo(
        web_app.get(), options_.iwa_options->location,
        web_app_info_.isolated_web_app_version(),
        web_app_info_.iwa_update_manifest_url,
        options_.iwa_options->integrity_block_data);

    HostContentSettingsMap* const host_content_settings_map =
        HostContentSettingsMapFactory::GetForProfile(&profile_.get());

    host_content_settings_map->SetContentSettingDefaultScope(
        web_app_info_.scope, web_app_info_.scope, ContentSettingsType::POPUPS,
        CONTENT_SETTING_ALLOW);
  }

  web_app->SetParentAppId(web_app_info_.parent_app_id);
  web_app->SetAdditionalSearchTerms(web_app_info_.additional_search_terms);

  bool is_app_suggested_for_migration =
      web_app->install_state() == proto::SUGGESTED_FROM_MIGRATION;
  if (is_app_suggested_for_migration) {
    CHECK_NE(options_.source, WebAppManagement::kSync)
        << " sync installs are not allowed for apps suggested from migration";
  }
  web_app->AddSource(options_.source);
  if (options_.source == WebAppManagement::kUserInstalled &&
      IsSyncEnabledForApps(&profile_.get()) &&
      !is_app_suggested_for_migration) {
    web_app->AddSource(WebAppManagement::kSync);
  }

  web_app->SetIsFromSyncAndPendingInstallation(false);
  web_app->SetLatestInstallSource(options_.install_surface);

  if (!web_app->generated_icon_fix().has_value()) {
    web_app->SetGeneratedIconFix(web_app_info_.generated_icon_fix);
  }

  if (web_app_info_.installed_by.has_value()) {
    web_app->AddInstalledByInfo(
        AppInstalledBy(clock_->Now(), web_app_info_.installed_by.value()));
  }

  WriteExternalConfigMapInfo(
      *web_app, options_.source, web_app_info_.is_placeholder,
      web_app_info_.install_url, web_app_info_.additional_policy_ids);

  if (options_.install_state !=
      proto::InstallState::INSTALLED_WITH_OS_INTEGRATION) {
    DCHECK(!(options_.add_to_applications_menu || options_.add_to_desktop ||
             options_.add_to_quick_launch_bar))
        << "Cannot create os hooks for a non-fully installed app";
  }

  std::optional<WebAppScope> old_scope;
  if (existing_web_app) {
    old_scope = existing_web_app->GetScope();
  }

  CommitCallback commit_callback =
      base::BindOnce(&FinalizeInstallJob::OnDatabaseCommitCompletedForInstall,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback_),
                     app_id, std::move(old_scope));

  // Ensure that the pending update info is always reset whenever Finalize*() is
  // called, to ensure that the state of icons on disk or new installs do not
  // have left over pending updates.
  if (options_.overwrite_existing_manifest_fields) {
    web_app->SetPendingUpdateInfo(std::nullopt);
  }

  if (options_.overwrite_existing_manifest_fields || !existing_web_app) {
    SetWebAppManifestFieldsAndWriteData(
        std::move(web_app), std::move(commit_callback),
        options_.skip_icon_writes_on_download_failure);
  } else {
    // Updates the web app with an additional source.
    CommitToSyncBridge(std::move(web_app), std::move(commit_callback),
                       /*success=*/true);
  }
}

void FinalizeInstallJob::UpdateIsolationDataAndResetPendingUpdateInfo(
    WebApp* web_app,
    const IsolatedWebAppStorageLocation& location,
    const IwaVersion& version,
    const std::optional<GURL>& iwa_update_manifest_url,
    std::optional<IsolatedWebAppIntegrityBlockData> integrity_block_data) {
  IsolationData::Builder builder(location, version);

  if (web_app->isolation_data()) {
    builder.PersistFieldsForUpdate(*web_app->isolation_data());
  }

  if (iwa_update_manifest_url) {
    builder.SetUpdateManifestUrl(*iwa_update_manifest_url);
  }

  if (integrity_block_data) {
    builder.SetIntegrityBlockData(std::move(*integrity_block_data));
  }

  web_app->SetIsolationData(std::move(builder).Build());
}

void FinalizeInstallJob::SetWebAppManifestFieldsAndWriteData(
    std::unique_ptr<WebApp> web_app,
    CommitCallback commit_callback,
    bool skip_icon_writes_on_download_failure) {
  const WebApp* existing_app = registrar().GetAppById(web_app->app_id());

  SetWebAppManifestFields(web_app_info_, *web_app,
                          skip_icon_writes_on_download_failure);
  AdjustAppStateBeforeCommit(existing_app, *web_app, *provider_);

  webapps::AppId app_id = web_app->app_id();
  auto write_translations_callback = base::BindOnce(
      &FinalizeInstallJob::WriteTranslations, weak_ptr_factory_.GetWeakPtr(),
      app_id, web_app_info_.translations);
  auto commit_to_sync_bridge_callback =
      base::BindOnce(&FinalizeInstallJob::CommitToSyncBridge,
                     weak_ptr_factory_.GetWeakPtr(), std::move(web_app));
  auto on_icon_write_complete_callback =
      base::BindOnce(std::move(write_translations_callback),
                     base::BindOnce(std::move(commit_to_sync_bridge_callback),
                                    std::move(commit_callback)));

  // Do not overwrite the icon data in the DB if icon downloading has failed. We
  // skip directly to writing translations and then writing the app via the
  // WebAppSyncBridge.
  if (skip_icon_writes_on_download_failure) {
    std::move(on_icon_write_complete_callback).Run(/*success=*/true);
  } else {
    IconBitmaps icon_bitmaps = web_app_info_.icon_bitmaps;
    ShortcutsMenuIconBitmaps shortcuts_menu_icon_bitmaps =
        web_app_info_.shortcuts_menu_icon_bitmaps;
    IconsMap other_icon_bitmaps = web_app_info_.other_icon_bitmaps;
    IconBitmaps trusted_icon_bitmaps = web_app_info_.trusted_icon_bitmaps;

    icon_manager().WriteData(
        app_id, std::move(icon_bitmaps), std::move(trusted_icon_bitmaps),
        std::move(shortcuts_menu_icon_bitmaps), std::move(other_icon_bitmaps),
        std::move(on_icon_write_complete_callback));
  }
}

void FinalizeInstallJob::WriteTranslations(
    const webapps::AppId& app_id,
    const base::flat_map<std::string, blink::Manifest::TranslationItem>&
        translations,
    CommitCallback commit_callback,
    bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!success) {
    std::move(commit_callback).Run(success);
    return;
  }
  translation_manager().WriteTranslations(app_id, translations,
                                          std::move(commit_callback));
}

void FinalizeInstallJob::CommitToSyncBridge(std::unique_ptr<WebApp> web_app,
                                            CommitCallback commit_callback,
                                            bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!success) {
    std::move(commit_callback).Run(success);
    return;
  }

  webapps::AppId app_id = web_app->app_id();

  ScopedRegistryUpdate update =
      sync_bridge().BeginUpdate(std::move(commit_callback));

  WebApp* app_to_override = update->UpdateApp(app_id);
  if (app_to_override) {
    *app_to_override = std::move(*web_app);
  } else {
    update->CreateApp(std::move(web_app));
  }
}

void FinalizeInstallJob::WriteExternalConfigMapInfo(
    WebApp& web_app,
    WebAppManagement::Type source,
    bool is_placeholder,
    GURL install_url,
    std::vector<std::string> additional_policy_ids) {
  DCHECK(!(source == WebAppManagement::Type::kSync && is_placeholder));
  DCHECK(!(source == WebAppManagement::Type::kUserInstalled && is_placeholder));
  if (source != WebAppManagement::Type::kSync &&
      source != WebAppManagement::Type::kUserInstalled &&
      !WebAppManagement::IsIwaType(source)) {
    web_app.AddPlaceholderInfoToManagementExternalConfigMap(source,
                                                            is_placeholder);
    if (install_url.is_valid()) {
      web_app.AddInstallURLToManagementExternalConfigMap(
          source, std::move(install_url));
    }
    for (const auto& policy_id : additional_policy_ids) {
      web_app.AddPolicyIdToManagementExternalConfigMap(source,
                                                       std::move(policy_id));
    }
  }
}

void FinalizeInstallJob::AdjustAppStateBeforeCommit(const WebApp* existing_app,
                                                    WebApp& web_app,
                                                    WebAppProvider& provider) {
  const auto& registrar = provider.registrar_unsafe();
  if (auto approval_state =
          AdjustFileHandlerUserApproval(registrar, existing_app, web_app)) {
    web_app.SetFileHandlerApprovalState(*approval_state);
  }

  // If the validated migration sources change, schedule a command to update
  // the pending migration info field for all web apps to reflect these
  // changes.
  if (base::FeatureList::IsEnabled(blink::features::kWebAppMigrationApi)) {
    auto old_sources = existing_app
                           ? existing_app->validated_migration_sources()
                           : std::vector<proto::WebAppMigrationSource>{};
    if (old_sources != web_app.validated_migration_sources()) {
      provider.scheduler().ScheduleResolveWebAppPendingMigrationInfo(
          base::DoNothing());
    }
  }
}

void FinalizeInstallJob::OnDatabaseCommitCompletedForInstall(
    InstallFinalizedCallback callback,
    webapps::AppId app_id,
    std::optional<WebAppScope> old_scope,
    bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!success) {
    lock_ = nullptr;
    resources_lock_ = nullptr;
    std::move(callback).Run(webapps::AppId(),
                            webapps::InstallResultCode::kWriteDataFailed);
    return;
  }

  const WebApp* web_app = registrar().GetAppById(app_id);
  // TODO(dmurph): Verify this check is not needed and remove after
  // isolation work is done. https://crbug.com/1298130
  if (!web_app) {
    lock_ = nullptr;
    resources_lock_ = nullptr;
    std::move(callback).Run(
        webapps::AppId(),
        webapps::InstallResultCode::kAppNotInRegistrarAfterCommit);
    return;
  }
  if (old_scope.has_value() && old_scope.value() != web_app->GetScope()) {
    registrar().NotifyWebAppEffectiveScopeChanged(app_id);
  }

  install_manager().NotifyWebAppInstalled(app_id);

  SynchronizeOsOptions synchronize_options;
  synchronize_options.add_shortcut_to_desktop = options_.add_to_desktop;
  synchronize_options.add_to_quick_launch_bar =
      options_.add_to_quick_launch_bar;

  switch (options_.source) {
    case WebAppManagement::kSystem:
    case WebAppManagement::kPolicy:
    case WebAppManagement::kIwaPolicy:
    case WebAppManagement::kDefault:
    case WebAppManagement::kOem:
    case WebAppManagement::kApsDefault:
    case WebAppManagement::kIwaShimlessRma:
      synchronize_options.reason = SHORTCUT_CREATION_AUTOMATED;
      break;
    case WebAppManagement::kKiosk:
    case WebAppManagement::kSubApp:
    case WebAppManagement::kWebAppStore:
    case WebAppManagement::kOneDriveIntegration:
    case WebAppManagement::kSync:
    case WebAppManagement::kUserInstalled:
    case WebAppManagement::kIwaUserInstalled:
      synchronize_options.reason = SHORTCUT_CREATION_BY_USER;
      break;
  }

  os_integration_manager().Synchronize(
      app_id,
      base::BindOnce(&FinalizeInstallJob::OnInstallHooksFinished,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     app_id),
      synchronize_options);
}

void FinalizeInstallJob::OnInstallHooksFinished(
    InstallFinalizedCallback callback,
    webapps::AppId app_id) {
  // Only notify that os hooks were added if the installation was a 'full'
  // installation.
  if (registrar().GetInstallState(app_id) ==
      proto::InstallState::INSTALLED_WITH_OS_INTEGRATION) {
    callback = std::move(callback).Then(
        base::BindOnce(&FinalizeInstallJob::NotifyWebAppInstalledWithOsHooks,
                       provider_, app_id));
  }
  lock_ = nullptr;
  resources_lock_ = nullptr;
  std::move(callback).Run(app_id,
                          webapps::InstallResultCode::kSuccessNewInstall);
}

void FinalizeInstallJob::NotifyWebAppInstalledWithOsHooks(
    WebAppProvider* provider,
    webapps::AppId app_id) {
  provider->install_manager().NotifyWebAppInstalledWithOsHooks(app_id);
}

bool& FinalizeInstallJob::DisableUserDisplayModeSyncMitigationsForTesting() {
  static bool disable = false;
  return disable;
}

WebAppRegistrar& FinalizeInstallJob::registrar() const {
  if (resources_lock_) {
    return resources_lock_->registrar();
  }
  return provider_->registrar_unsafe();
}

WebAppSyncBridge& FinalizeInstallJob::sync_bridge() const {
  if (resources_lock_) {
    return resources_lock_->sync_bridge();
  }
  return provider_->sync_bridge_unsafe();
}

// TODO(crbug.com/259703817): This method is temporary, this should be removed
// once refactoring is complete and the job is solely dependent on the lock for
// these resources.
WebAppInstallManager& FinalizeInstallJob::install_manager() const {
  if (resources_lock_) {
    return resources_lock_->install_manager();
  }
  return provider_->install_manager();
}

// TODO(crbug.com/259703817): This method is temporary, this should be removed
// once refactoring is complete and the job is solely dependent on the lock for
// these resources.
WebAppIconManager& FinalizeInstallJob::icon_manager() const {
  if (resources_lock_) {
    return resources_lock_->icon_manager();
  }
  return provider_->icon_manager();
}

// TODO(crbug.com/259703817): This method is temporary, this should be removed
// once refactoring is complete and the job is solely dependent on the lock for
// these resources.
WebAppTranslationManager& FinalizeInstallJob::translation_manager() const {
  if (resources_lock_) {
    return resources_lock_->translation_manager();
  }
  return provider_->translation_manager();
}

// TODO(crbug.com/259703817): This method is temporary, this should be removed
// once refactoring is complete and the job is solely dependent on the lock for
// these resources.
OsIntegrationManager& FinalizeInstallJob::os_integration_manager() const {
  if (resources_lock_) {
    return resources_lock_->os_integration_manager();
  }
  return provider_->os_integration_manager();
}

// TODO(crbug.com/259703817): This method is temporary, this should be removed
// once refactoring is complete and the job is solely dependent on the lock for
// these resources.
WebAppOriginAssociationManager& FinalizeInstallJob::origin_association_manager()
    const {
  if (lock_) {
    return lock_->origin_association_manager();
  }
  return provider_->origin_association_manager();
}

}  // namespace web_app
