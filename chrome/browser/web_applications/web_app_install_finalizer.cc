// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_install_finalizer.h"

#include <map>
#include <utility>

#include "ash/constants/web_app_id_constants.h"
#include "base/barrier_callback.h"
#include "base/check_is_test.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/not_fatal_until.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/types/optional_ref.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/commands/web_app_uninstall_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_integrity_block_data.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_storage_location.h"
#include "chrome/browser/web_applications/jobs/uninstall/remove_install_source_job.h"
#include "chrome/browser/web_applications/jobs/uninstall/remove_install_url_job.h"
#include "chrome/browser/web_applications/jobs/uninstall/remove_web_app_job.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcuts_menu.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_generator.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_origin_association_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_translation_manager.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/sync/protocol/web_app_specifics.pb.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/origin.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/system_web_apps/types/system_web_app_data.h"
#endif

namespace web_app {
namespace {

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
      return false;
    case InstallSource::COUNT:
      NOTREACHED_IN_MIGRATION();
      return false;
  }
}

#if BUILDFLAG(IS_CHROMEOS)
// When web apps are added to sync on ChromeOS the value of
// user_display_mode_default should be set in certain cases to avoid poor sync
// install states on pre-M122 devices and non-CrOS devices with particular web
// apps.
// See switch for specific cases being mitigated against.
// See go/udm-desync#bookmark=id.cg753kjyrruo for design doc.
// TODO(b/320771282): Add automated tests.
void ApplyUserDisplayModeSyncMitigations(
    const WebAppInstallFinalizer::FinalizeOptions& options,
    WebApp& web_app) {
  // Guaranteed by EnsureAppsHaveUserDisplayModeForCurrentPlatform().
  CHECK(web_app.sync_proto().has_user_display_mode_cros(),
        base::NotFatalUntil::M125);

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
      if (!base::FeatureList::IsEnabled(
              kUserDisplayModeSyncBrowserMitigation)) {
        return;
      }

      // Pre-M122 CrOS devices use the user_display_mode_default sync field
      // instead of user_display_mode_cros. If user_display_mode_default is ever
      // unset they will fallback to using kStandalone even if
      // user_display_mode_cros is set to kBrowser. This mitigation ensures
      // user_display_mode_default is set to kBrowser for these devices. Example
      // user journey:
      // - Install web app as browser shortcut on post-M122 CrOS device.
      // - Sync installation to pre-M122 CrOS device.
      // - Check that it is synced as a browser shortcut.
      // TODO(b/321617981): Remove when there are sufficiently few pre-M122 CrOS
      // devices in circulation.
      sync_proto.set_user_display_mode_default(
          sync_pb::WebAppSpecifics_UserDisplayMode_BROWSER);
      break;

    case sync_pb::WebAppSpecifics_UserDisplayMode_STANDALONE: {
      if (!base::FeatureList::IsEnabled(
              kUserDisplayModeSyncStandaloneMitigation)) {
        return;
      }

      // Ensure standalone averse apps don't get defaulted to kStandalone on
      // non-CrOS devices via sync.
      // Example user journey:
      // - Install Google Docs as a standalone web app.
      // - Sync installation to non-CrOS device.
      // - Check that it is synced as a browser shortcut.
      // TODO(b/321617972): Remove when Windows/Mac/Linux support for tabbed web
      // apps is in sufficient circulation.
      bool is_standalone_averse_app = web_app.app_id() == kGoogleDocsAppId ||
                                      web_app.app_id() == kGoogleSheetsAppId ||
                                      web_app.app_id() == kGoogleSlidesAppId;
      if (!is_standalone_averse_app) {
        break;
      }
      sync_proto.set_user_display_mode_default(
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

  web_app.SetSyncProto(std::move(sync_proto));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace

WebAppInstallFinalizer::FinalizeOptions::IwaOptions::IwaOptions(
    IsolatedWebAppStorageLocation location,
    std::optional<IsolatedWebAppIntegrityBlockData> integrity_block_data)
    : location(std::move(location)),
      integrity_block_data(std::move(integrity_block_data)) {}

WebAppInstallFinalizer::FinalizeOptions::IwaOptions::~IwaOptions() = default;

WebAppInstallFinalizer::FinalizeOptions::IwaOptions::IwaOptions(
    const IwaOptions&) = default;

WebAppInstallFinalizer::FinalizeOptions::FinalizeOptions(
    webapps::WebappInstallSource install_surface)
    : source(ConvertInstallSurfaceToWebAppSource(install_surface)),
      install_surface(install_surface) {}

WebAppInstallFinalizer::FinalizeOptions::~FinalizeOptions() = default;

WebAppInstallFinalizer::FinalizeOptions::FinalizeOptions(
    const FinalizeOptions&) = default;

WebAppInstallFinalizer::WebAppInstallFinalizer(Profile* profile)
    : profile_(profile) {}

WebAppInstallFinalizer::~WebAppInstallFinalizer() = default;

void WebAppInstallFinalizer::FinalizeInstall(
    const WebAppInstallInfo& web_app_info,
    const FinalizeOptions& options,
    InstallFinalizedCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // TODO(crbug.com/40693380): Implement a before-start queue in
  // WebAppInstallManager and replace this runtime error in
  // WebAppInstallFinalizer with DCHECK(started_).
  if (!started_) {
    std::move(callback).Run(
        webapps::AppId(), webapps::InstallResultCode::kWebAppProviderNotReady);
    return;
  }

  webapps::ManifestId manifest_id = web_app_info.manifest_id();

  // parent_app_manifest_id can only exist if installing as a sub-app.
  CHECK((options.install_surface == webapps::WebappInstallSource::SUB_APP &&
         web_app_info.parent_app_manifest_id.has_value()) ||
        (options.install_surface != webapps::WebappInstallSource::SUB_APP &&
         !web_app_info.parent_app_manifest_id.has_value()));

  webapps::AppId app_id = GenerateAppIdFromManifestId(
      manifest_id, web_app_info.parent_app_manifest_id);
  OnDidGetWebAppOriginAssociations origin_association_validated_callback =
      base::BindOnce(&WebAppInstallFinalizer::OnOriginAssociationValidated,
                     weak_ptr_factory_.GetWeakPtr(), web_app_info.Clone(),
                     options, std::move(callback), app_id);

  if (options.skip_origin_association_validation ||
      web_app_info.scope_extensions.empty() ||
      web_app_info.validated_scope_extensions.has_value()) {
    std::move(origin_association_validated_callback).Run(ScopeExtensions());
    return;
  }

  provider_->origin_association_manager().GetWebAppOriginAssociations(
      manifest_id, web_app_info.scope_extensions,
      std::move(origin_association_validated_callback));
}

void WebAppInstallFinalizer::OnOriginAssociationValidated(
    WebAppInstallInfo web_app_info,
    FinalizeOptions options,
    InstallFinalizedCallback callback,
    webapps::AppId app_id,
    ScopeExtensions validated_scope_extensions) {
  const WebApp* existing_web_app =
      provider_->registrar_unsafe().GetAppById(app_id);
  std::unique_ptr<WebApp> web_app;
  if (existing_web_app) {
    web_app = std::make_unique<WebApp>(*existing_web_app);
  } else {
    // TODO(b/344718166): Ensure that manifest_id corresponds to app_id here.
    web_app = std::make_unique<WebApp>(app_id);
    web_app->SetInstallState(proto::SUGGESTED_FROM_ANOTHER_DEVICE);
    // Ensure `web_app` has a start_url and manifest_id set before other calls
    // that depend on state being complete, eg. `WebApp::sync_proto()`.
    web_app->SetStartUrl(web_app_info.start_url());
    web_app->SetManifestId(web_app_info.manifest_id());
  }

  web_app->SetValidatedScopeExtensions(validated_scope_extensions);

  const base::Time now_time = base::Time::Now();

  // The UI may initiate a full install to overwrite the existing
  // non-locally-installed app. Therefore, `install_state` can be
  // promoted to `INSTALLED_WITH_OS_INTEGRATION`, but not vice versa.
  if (options.install_state ==
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
      options.install_state ==
          proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION) {
    web_app->SetInstallState(
        proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION);
  }

  // Set |user_display_mode| and any user-controllable fields here if this
  // install is user initiated or it's a new app.
  if (ShouldInstallOverwriteUserDisplayMode(options.install_surface) ||
      !existing_web_app) {
    DCHECK(web_app_info.user_display_mode.has_value());
    web_app->SetUserDisplayMode(*web_app_info.user_display_mode);
  }
#if BUILDFLAG(IS_CHROMEOS)
  ApplyUserDisplayModeSyncMitigations(options, *web_app);
#endif  // BUILDFLAG(IS_CHROMEOS)

  // `WebApp::chromeos_data` has a default value already. Only override if the
  // caller provided a new value.
  if (options.chromeos_data.has_value())
    web_app->SetWebAppChromeOsData(options.chromeos_data.value());

  if (provider_->policy_manager().IsWebAppInDisabledList(app_id) &&
      web_app->chromeos_data().has_value() &&
      !web_app->chromeos_data()->is_disabled) {
    std::optional<WebAppChromeOsData> cros_data = web_app->chromeos_data();
    cros_data->is_disabled = true;
    web_app->SetWebAppChromeOsData(std::move(cros_data));
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // `WebApp::system_web_app_data` has a default value already. Only override if
  // the caller provided a new value.
  if (options.system_web_app_data.has_value()) {
    web_app->client_data()->system_web_app_data =
        options.system_web_app_data.value();
  }
#endif

  if (options.iwa_options) {
    UpdateIsolationDataAndResetPendingUpdateInfo(
        web_app.get(), options.iwa_options->location,
        web_app_info.isolated_web_app_version,
        options.iwa_options->integrity_block_data);
  }

  web_app->SetParentAppId(web_app_info.parent_app_id);
  web_app->SetAdditionalSearchTerms(web_app_info.additional_search_terms);
  web_app->AddSource(options.source);
  if (base::FeatureList::IsEnabled(
          features::kWebAppDontAddExistingAppsToSync) &&
      options.source == WebAppManagement::kUserInstalled &&
      IsSyncEnabledForApps(profile_)) {
    web_app->AddSource(WebAppManagement::kSync);
  }
  web_app->SetIsFromSyncAndPendingInstallation(false);
  web_app->SetLatestInstallSource(options.install_surface);

  if (!web_app->generated_icon_fix().has_value()) {
    web_app->SetGeneratedIconFix(web_app_info.generated_icon_fix);
  }

  WriteExternalConfigMapInfo(
      *web_app, options.source, web_app_info.is_placeholder,
      web_app_info.install_url, web_app_info.additional_policy_ids);

  if (options.install_state !=
      proto::InstallState::INSTALLED_WITH_OS_INTEGRATION) {
    DCHECK(!(options.add_to_applications_menu || options.add_to_desktop ||
             options.add_to_quick_launch_bar))
        << "Cannot create os hooks for a non-fully installed app";
  }

  CommitCallback commit_callback = base::BindOnce(
      &WebAppInstallFinalizer::OnDatabaseCommitCompletedForInstall,
      weak_ptr_factory_.GetWeakPtr(), std::move(callback), app_id, options);

  if (options.overwrite_existing_manifest_fields || !existing_web_app) {
    SetWebAppManifestFieldsAndWriteData(
        web_app_info, std::move(web_app), std::move(commit_callback),
        options.skip_icon_writes_on_download_failure);
  } else {
    // Updates the web app with an additional source.
    CommitToSyncBridge(std::move(web_app), std::move(commit_callback),
                       /*success=*/true);
  }
}

bool WebAppInstallFinalizer::CanReparentTab(const webapps::AppId& app_id,
                                            bool shortcut_created) const {
  // Reparent the web contents into its own window only if that is the
  // app's launch type.
  DCHECK(provider_);
  if (provider_->registrar_unsafe().GetAppUserDisplayMode(app_id) ==
      mojom::UserDisplayMode::kBrowser) {
    return false;
  }

  return provider_->ui_manager().CanReparentAppTabToWindow(app_id,
                                                           shortcut_created);
}

void WebAppInstallFinalizer::ReparentTab(const webapps::AppId& app_id,
                                         bool shortcut_created,
                                         content::WebContents* web_contents) {
  DCHECK(web_contents);
  provider_->ui_manager().ReparentAppTabToWindow(web_contents, app_id,
                                                 shortcut_created);
}

void WebAppInstallFinalizer::FinalizeUpdate(
    const WebAppInstallInfo& web_app_info,
    InstallFinalizedCallback callback) {
  CHECK(started_);
  webapps::ManifestId manifest_id = web_app_info.manifest_id();
  const webapps::AppId app_id = GenerateAppIdFromManifestId(manifest_id);
  const WebApp* existing_web_app =
      provider_->registrar_unsafe().GetAppById(app_id);

  if (!existing_web_app ||
      existing_web_app->is_from_sync_and_pending_installation() ||
      app_id != existing_web_app->app_id()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), webapps::AppId(),
                                  webapps::InstallResultCode::kWebAppDisabled));
    return;
  }

  CommitCallback commit_callback = base::BindOnce(
      &WebAppInstallFinalizer::OnDatabaseCommitCompletedForUpdate,
      weak_ptr_factory_.GetWeakPtr(), std::move(callback), app_id,
      provider_->registrar_unsafe().GetAppShortName(app_id),
      GetFileHandlerUpdateAction(app_id, web_app_info), web_app_info.Clone());

  auto web_app = std::make_unique<WebApp>(*existing_web_app);
  if (web_app->isolation_data().has_value()) {
    const std::optional<IsolationData::PendingUpdateInfo>& pending_update_info =
        web_app->isolation_data()->pending_update_info();
    CHECK(pending_update_info.has_value())
        << "Isolated Web Apps can only be updated if "
           "`IsolationData::PendingUpdateInfo` is set.";
    CHECK_EQ(web_app_info.isolated_web_app_version,
             pending_update_info->version);
    UpdateIsolationDataAndResetPendingUpdateInfo(
        web_app.get(), pending_update_info->location,
        pending_update_info->version,
        pending_update_info->integrity_block_data);
  }

  // Prepare copy-on-write to update existing app.
  // This is not reached unless the data obtained from the manifest
  // update process is valid, so an invariant of the system is that
  // icons are valid here.
  SetWebAppManifestFieldsAndWriteData(
      web_app_info, std::move(web_app), std::move(commit_callback),
      /*skip_icon_writes_on_download_failure=*/false);
}

void WebAppInstallFinalizer::SetProvider(base::PassKey<WebAppProvider>,
                                         WebAppProvider& provider) {
  provider_ = &provider;
}

void WebAppInstallFinalizer::Start() {
  DCHECK(!started_);
  started_ = true;
}

void WebAppInstallFinalizer::Shutdown() {
  started_ = false;
  // TODO(crbug.com/40810770): Turn WebAppInstallFinalizer into a command so it
  // can properly call callbacks on shutdown instead of dropping them on
  // shutdown.
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void WebAppInstallFinalizer::UpdateIsolationDataAndResetPendingUpdateInfo(
    WebApp* web_app,
    const IsolatedWebAppStorageLocation& location,
    const base::Version& version,
    std::optional<IsolatedWebAppIntegrityBlockData> integrity_block_data) {
  CHECK(version.IsValid());

  IsolationData::Builder builder(location, version);
  if (web_app->isolation_data()) {
    builder.PersistFieldsForUpdate(*web_app->isolation_data());
  }

  if (integrity_block_data) {
    builder.SetIntegrityBlockData(std::move(*integrity_block_data));
  }

  web_app->SetIsolationData(std::move(builder).Build());
}

void WebAppInstallFinalizer::SetWebAppManifestFieldsAndWriteData(
    const WebAppInstallInfo& web_app_info,
    std::unique_ptr<WebApp> web_app,
    CommitCallback commit_callback,
    bool skip_icon_writes_on_download_failure) {
  SetWebAppManifestFields(web_app_info, *web_app,
                          skip_icon_writes_on_download_failure);

  webapps::AppId app_id = web_app->app_id();
  auto write_translations_callback = base::BindOnce(
      &WebAppInstallFinalizer::WriteTranslations,
      weak_ptr_factory_.GetWeakPtr(), app_id, web_app_info.translations);
  auto commit_to_sync_bridge_callback =
      base::BindOnce(&WebAppInstallFinalizer::CommitToSyncBridge,
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
    IconBitmaps icon_bitmaps = web_app_info.icon_bitmaps;
    ShortcutsMenuIconBitmaps shortcuts_menu_icon_bitmaps =
        web_app_info.shortcuts_menu_icon_bitmaps;
    IconsMap other_icon_bitmaps = web_app_info.other_icon_bitmaps;

    provider_->icon_manager().WriteData(
        app_id, std::move(icon_bitmaps), std::move(shortcuts_menu_icon_bitmaps),
        std::move(other_icon_bitmaps),
        std::move(on_icon_write_complete_callback));
  }
}

void WebAppInstallFinalizer::WriteTranslations(
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
  provider_->translation_manager().WriteTranslations(
      app_id, translations, std::move(commit_callback));
}

void WebAppInstallFinalizer::CommitToSyncBridge(std::unique_ptr<WebApp> web_app,
                                                CommitCallback commit_callback,
                                                bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!success) {
    std::move(commit_callback).Run(success);
    return;
  }

  webapps::AppId app_id = web_app->app_id();

  ScopedRegistryUpdate update =
      provider_->sync_bridge_unsafe().BeginUpdate(std::move(commit_callback));

  WebApp* app_to_override = update->UpdateApp(app_id);
  if (app_to_override) {
    *app_to_override = std::move(*web_app);
  } else {
    update->CreateApp(std::move(web_app));
  }
}

void WebAppInstallFinalizer::OnDatabaseCommitCompletedForInstall(
    InstallFinalizedCallback callback,
    webapps::AppId app_id,
    FinalizeOptions finalize_options,
    bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!success) {
    std::move(callback).Run(webapps::AppId(),
                            webapps::InstallResultCode::kWriteDataFailed);
    return;
  }

  provider_->install_manager().NotifyWebAppInstalled(app_id);

  const WebApp* web_app = provider_->registrar_unsafe().GetAppById(app_id);
  // TODO(dmurph): Verify this check is not needed and remove after
  // isolation work is done. https://crbug.com/1298130
  if (!web_app) {
    std::move(callback).Run(
        webapps::AppId(),
        webapps::InstallResultCode::kAppNotInRegistrarAfterCommit);
    return;
  }

  SynchronizeOsOptions synchronize_options;
  synchronize_options.add_shortcut_to_desktop = finalize_options.add_to_desktop;
  synchronize_options.add_to_quick_launch_bar =
      finalize_options.add_to_quick_launch_bar;

  switch (finalize_options.source) {
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

  provider_->os_integration_manager().Synchronize(
      app_id,
      base::BindOnce(&WebAppInstallFinalizer::OnInstallHooksFinished,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     app_id),
      synchronize_options);
}

void WebAppInstallFinalizer::OnInstallHooksFinished(
    InstallFinalizedCallback callback,
    webapps::AppId app_id) {
  // Only notify that os hooks were added if the installation was a 'full'
  // installation.
  if (provider_->registrar_unsafe().IsInstallState(
          app_id, {proto::InstallState::INSTALLED_WITH_OS_INTEGRATION})) {
    callback = std::move(callback).Then(base::BindOnce(
        &WebAppInstallFinalizer::NotifyWebAppInstalledWithOsHooks,
        weak_ptr_factory_.GetWeakPtr(), app_id));
  }
  std::move(callback).Run(app_id,
                          webapps::InstallResultCode::kSuccessNewInstall);
}

void WebAppInstallFinalizer::NotifyWebAppInstalledWithOsHooks(
    webapps::AppId app_id) {
  provider_->install_manager().NotifyWebAppInstalledWithOsHooks(app_id);
}

bool WebAppInstallFinalizer::ShouldUpdateOsHooks(const webapps::AppId& app_id) {
#if BUILDFLAG(IS_CHROMEOS)
  // OS integration should always be enabled on ChromeOS.
  return true;
#else
  // If the app being updated was installed by default and not also manually
  // installed by the user or an enterprise policy, disable os integration.
  return !provider_->registrar_unsafe().WasInstalledByDefaultOnly(app_id);
#endif  // BUILDFLAG(IS_CHROMEOS)
}

void WebAppInstallFinalizer::OnDatabaseCommitCompletedForUpdate(
    InstallFinalizedCallback callback,
    webapps::AppId app_id,
    std::string old_name,
    FileHandlerUpdateAction file_handlers_need_os_update,
    const WebAppInstallInfo& web_app_info,
    bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!success) {
    std::move(callback).Run(webapps::AppId(),
                            webapps::InstallResultCode::kWriteDataFailed);
    return;
  }

  if (!ShouldUpdateOsHooks(app_id)) {
    provider_->install_manager().NotifyWebAppManifestUpdated(app_id);
    std::move(callback).Run(
        app_id, webapps::InstallResultCode::kSuccessAlreadyInstalled);
    return;
  }

  provider_->os_integration_manager().Synchronize(
      app_id, base::BindOnce(&WebAppInstallFinalizer::OnUpdateHooksFinished,
                             weak_ptr_factory_.GetWeakPtr(),
                             std::move(callback), app_id));
}

void WebAppInstallFinalizer::OnUpdateHooksFinished(
    InstallFinalizedCallback callback,
    webapps::AppId app_id) {
  provider_->install_manager().NotifyWebAppManifestUpdated(app_id);
  std::move(callback).Run(app_id,
                          webapps::InstallResultCode::kSuccessAlreadyInstalled);
}

void WebAppInstallFinalizer::WriteExternalConfigMapInfo(
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

FileHandlerUpdateAction WebAppInstallFinalizer::GetFileHandlerUpdateAction(
    const webapps::AppId& app_id,
    const WebAppInstallInfo& new_web_app_info) {
  if (provider_->registrar_unsafe().GetAppFileHandlerApprovalState(app_id) ==
      ApiApprovalState::kDisallowed) {
    return FileHandlerUpdateAction::kNoUpdate;
  }

  // TODO(crbug.com/40176713): Consider trying to re-use the comparison
  // results from the ManifestUpdateDataFetchCommand.
  const apps::FileHandlers* old_handlers =
      provider_->registrar_unsafe().GetAppFileHandlers(app_id);
  DCHECK(old_handlers);
  if (*old_handlers == new_web_app_info.file_handlers)
    return FileHandlerUpdateAction::kNoUpdate;

  return FileHandlerUpdateAction::kUpdate;
}

}  // namespace web_app
