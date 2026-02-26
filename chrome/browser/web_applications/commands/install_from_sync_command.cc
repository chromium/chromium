// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/install_from_sync_command.h"

#include <memory>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/to_string.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/generated_icon_fix_util.h"
#include "chrome/browser/web_applications/install_bounce_metric.h"
#include "chrome/browser/web_applications/jobs/finalize_install_job.h"
#include "chrome/browser/web_applications/jobs/manifest_to_web_app_install_info_job.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_with_app_lock.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_operations.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"
#include "chrome/browser/web_applications/web_contents/web_contents_manager.h"
#include "chrome/common/chrome_features.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/web_contents/web_app_url_loader.h"
#include "content/public/browser/web_contents.h"

namespace web_app {

namespace {

FinalizeJobOptions GetFinalizerOptionForSyncInstall() {
  FinalizeJobOptions finalize_options(webapps::WebappInstallSource::SYNC);
  finalize_options.overwrite_existing_manifest_fields = true;
  // If app is not locally installed then no OS integration like OS shortcuts.
  finalize_options.install_state =
      AreAppsLocallyInstalledBySync()
          ? proto::InstallState::INSTALLED_WITH_OS_INTEGRATION
          : proto::InstallState::SUGGESTED_FROM_ANOTHER_DEVICE;
  finalize_options.add_to_applications_menu = AreAppsLocallyInstalledBySync();
  finalize_options.add_to_desktop = AreAppsLocallyInstalledBySync();
  // Never add the app to the quick launch bar after sync.
  finalize_options.add_to_quick_launch_bar = false;
  if (IsChromeOsDataMandatory()) {
    finalize_options.chromeos_data.emplace();
    finalize_options.chromeos_data->show_in_launcher =
        AreAppsLocallyInstalledBySync();
  }
  return finalize_options;
}

// On ChromeOS, always perform installation by fetching the manifest, while on
// other platforms, always install from fallback based on whether trusted icons
// are enabled.
bool ShouldInstallFallbackNoManifestFetching() {
#if BUILDFLAG(IS_CHROMEOS)
  return false;
#else
  return true;
#endif  // BUILDFLAG(IS_CHROMEOS)
}

}  // namespace

InstallFromSyncCommand::Params::~Params() = default;

InstallFromSyncCommand::Params::Params(
    const webapps::AppId& app_id,
    const webapps::ManifestId& manifest_id,
    const GURL& start_url,
    const std::string& title,
    const GURL& scope,
    const std::optional<SkColor>& theme_color,
    const std::optional<mojom::UserDisplayMode>& user_display_mode,
    const std::vector<apps::IconInfo>& manifest_icons,
    const std::vector<apps::IconInfo>& trusted_icons,
    const std::optional<webapps::ManifestId>& migrated_from_manifest_id)
    : app_id(app_id),
      manifest_id(manifest_id),
      start_url(start_url),
      title(title),
      scope(scope),
      theme_color(theme_color),
      user_display_mode(user_display_mode),
      manifest_icons(manifest_icons),
      trusted_icons(trusted_icons),
      migrated_from_manifest_id(migrated_from_manifest_id) {
  CHECK(!app_id.empty());
  CHECK(manifest_id.is_valid());
  CHECK(!manifest_id.is_empty());
}

InstallFromSyncCommand::Params::Params(const Params&) = default;

InstallFromSyncCommand::InstallFromSyncCommand(
    Profile* profile,
    const Params& params,
    OnceInstallCallback install_callback)
    : WebAppCommand<SharedWebContentsWithAppLock,
                    const webapps::AppId&,
                    webapps::InstallResultCode>(
          "InstallFromSyncCommand",
          SharedWebContentsWithAppLockDescription(
              params.migrated_from_manifest_id
                  ? base::flat_set<
                        webapps::AppId>{params.app_id,
                                        GenerateAppIdFromManifestId(
                                            *params.migrated_from_manifest_id)}
                  : base::flat_set<webapps::AppId>{params.app_id}),
          std::move(install_callback),
          /*args_for_shutdown=*/
          std::make_tuple(params.app_id,
                          webapps::InstallResultCode::
                              kCancelledOnWebAppProviderShuttingDown)),
      profile_(profile),
      params_(params) {
#if BUILDFLAG(IS_CHROMEOS)
  DCHECK(AreAppsLocallyInstalledBySync());
#endif
  DCHECK(params_.start_url.is_valid());
  if (params_.migrated_from_manifest_id) {
    source_app_id_ =
        GenerateAppIdFromManifestId(*params_.migrated_from_manifest_id);
  }
  fallback_install_info_ = std::make_unique<WebAppInstallInfo>(
      params_.manifest_id, params_.start_url);
  fallback_install_info_->title = base::UTF8ToUTF16(params_.title);
  fallback_install_info_->user_display_mode = params_.user_display_mode;
  fallback_install_info_->scope = params_.scope;
  fallback_install_info_->theme_color = params_.theme_color;
  fallback_install_info_->manifest_icons = params_.manifest_icons;
  fallback_install_info_->trusted_icons = params_.trusted_icons;
  GetMutableDebugValue().Set("app_id", params_.app_id);
  GetMutableDebugValue().Set("manifest_id", params_.manifest_id.spec());
  if (params.migrated_from_manifest_id) {
    GetMutableDebugValue().Set("source_app_id", *source_app_id_);
    GetMutableDebugValue().Set("migrated_from_manifest_id",
                               params.migrated_from_manifest_id->spec());
  }
  GetMutableDebugValue().Set("title", params_.title);
  GetMutableDebugValue().Set(
      "user_display_mode",
      params_.user_display_mode
          ? base::ToString(params_.user_display_mode.value())
          : "<unset>");
  GetMutableDebugValue().Set("scope", params_.scope.spec());
  GetMutableDebugValue().Set("start_url", params_.start_url.spec());
  GetMutableDebugValue().Set("fallback_install", false);
}

InstallFromSyncCommand::~InstallFromSyncCommand() = default;

void InstallFromSyncCommand::StartWithLock(
    std::unique_ptr<SharedWebContentsWithAppLock> lock) {
  lock_ = std::move(lock);
  url_loader_ = lock_->web_contents_manager().CreateUrlLoader();
  data_retriever_ = lock_->web_contents_manager().CreateDataRetriever();

  if (ShouldInstallFallbackNoManifestFetching()) {
    InstallFallback(
        webapps::InstallResultCode::kFallbackInstallUsingTrustedIcons);
    return;
  }

  url_loader_->LoadUrl(
      params_.start_url, &lock_->shared_web_contents(),
      webapps::WebAppUrlLoader::UrlComparison::kIgnoreQueryParamsAndRef,
      base::BindOnce(
          &InstallFromSyncCommand::OnWebAppUrlLoadedGetWebAppInstallInfo,
          weak_ptr_factory_.GetWeakPtr()));
}

void InstallFromSyncCommand::SetFallbackTriggeredForTesting(
    base::OnceCallback<void(webapps::InstallResultCode code)> callback) {
  fallback_triggered_for_testing_ = std::move(callback);
}

void InstallFromSyncCommand::OnWebAppUrlLoadedGetWebAppInstallInfo(
    webapps::WebAppUrlLoaderResult result) {
  GetMutableDebugValue().Set("WebAppUrlLoader::Result", base::ToString(result));

  if (result == webapps::WebAppUrlLoaderResult::kRedirectedUrlLoaded) {
    InstallFallback(webapps::InstallResultCode::kInstallURLRedirected);
    return;
  }

  if (result == webapps::WebAppUrlLoaderResult::kFailedPageTookTooLong) {
    InstallFallback(webapps::InstallResultCode::kInstallURLLoadTimeOut);
    return;
  }

  if (result != webapps::WebAppUrlLoaderResult::kUrlLoaded) {
    InstallFallback(webapps::InstallResultCode::kInstallURLLoadFailed);
    return;
  }

  data_retriever_->GetWebAppInstallInfo(
      &lock_->shared_web_contents(),
      base::BindOnce(&InstallFromSyncCommand::OnGetWebAppInstallInfo,
                     weak_ptr_factory_.GetWeakPtr()));
}

void InstallFromSyncCommand::OnGetWebAppInstallInfo(
    std::unique_ptr<WebAppInstallInfo> web_app_info) {
  if (!web_app_info) {
    InstallFallback(webapps::InstallResultCode::kGetWebAppInstallInfoFailed);
    return;
  }

  // Populate fallback info with the data retrieved from `GetWebAppInstallInfo`.
  // `trusted_icons` do not need to be updated here because the
  // `WebAppInstallInfo` obtained here is from the page metadata.
  fallback_install_info_->description = web_app_info->description;
  if (!web_app_info->manifest_icons.empty()) {
    fallback_install_info_->manifest_icons = web_app_info->manifest_icons;
  }
  fallback_install_info_->mobile_capable = web_app_info->mobile_capable;

  data_retriever_->CheckInstallabilityAndRetrieveManifest(
      &lock_->shared_web_contents(),
      base::BindOnce(&InstallFromSyncCommand::OnDidPerformInstallableCheck,
                     weak_ptr_factory_.GetWeakPtr(), std::move(web_app_info)));
}

void InstallFromSyncCommand::OnDidPerformInstallableCheck(
    std::unique_ptr<WebAppInstallInfo> page_info,
    blink::mojom::ManifestPtr opt_manifest,
    bool valid_manifest_for_web_app,
    webapps::InstallableStatusCode error_code) {
  if (!opt_manifest) {
    InstallFallback(webapps::InstallResultCode::kExpectedAppIdCheckFailed);
    return;
  }

  page_info->title = base::UTF8ToUTF16(params_.title);

  // If the page doesn't have a favicon, then the icon fetcher will hang
  // forever.
  // TODO(crbug.com/40226606): Allow favicons without waiting for them to
  // be updated on the page.
  manifest_to_install_info_job_ =
      ManifestToWebAppInstallInfoJob::CreateAndStart(
          *opt_manifest, *data_retriever_.get(),
          /*background_installation=*/true, webapps::WebappInstallSource::SYNC,
          lock_->shared_web_contents().GetWeakPtr(), [](IconUrlSizeSet&) {},
          GetMutableDebugValue(),
          base::BindOnce(
              &InstallFromSyncCommand::
                  OnWebAppInstallInfoRetrievedMergeAndFinalizeInstall,
              weak_ptr_factory_.GetWeakPtr()),
          WebAppInstallInfoConstructOptions{}, page_info->Clone());
}

void InstallFromSyncCommand::
    OnWebAppInstallInfoRetrievedMergeAndFinalizeInstall(
        std::unique_ptr<WebAppInstallInfo> install_info) {
  // Ensure that the manifest linked is the right one.
  webapps::AppId generated_app_id =
      GenerateAppIdFromManifestId(install_info->manifest_id());
  if (params_.app_id != generated_app_id) {
    // Add the error to the log.
    base::DictValue expected_id_error;
    expected_id_error.Set("expected_app_id", params_.app_id);
    expected_id_error.Set("app_id", generated_app_id);
    GetMutableDebugValue().Set("app_id_error", std::move(expected_id_error));

    InstallFallback(webapps::InstallResultCode::kExpectedAppIdCheckFailed);
    return;
  }

  CHECK(!install_info_);
  install_info_ = std::move(install_info);
  install_info_->user_display_mode = params_.user_display_mode;
  CheckForMigrationAndGatherInfo(FinalizeMode::kNormalWebAppInfo);
}

void InstallFromSyncCommand::OnIconsRetrievedForFallbackInfo(
    IconsDownloadedResult result,
    IconsMap icons_map,
    DownloadedIconsHttpResults icons_http_results) {
  PopulateProductIcons(fallback_install_info_.get(), &icons_map);
  if (fallback_install_info_->is_generated_icon) {
    GetMutableDebugValue().Set("fallback_is_generated_icon", true);
  }
  PopulateTrustedIconBitmaps(*fallback_install_info_.get(), icons_map);
  PopulateOtherIcons(fallback_install_info_.get(), icons_map);

  RecordDownloadedIconsHttpResultsCodeClass(
      "WebApp.Icon.HttpStatusCodeClassOnSync", result, icons_http_results);
  UMA_HISTOGRAM_ENUMERATION("WebApp.Icon.DownloadedResultOnSync", result);
  RecordDownloadedIconHttpStatusCodes(
      "WebApp.Icon.DownloadedHttpStatusCodeOnSync", icons_http_results);
  base::DictValue icon_errors =
      LogDownloadedIconsErrors(result, icons_map, icons_http_results);
  if (!icon_errors.empty()) {
    GetMutableDebugValue().Set("icon_errors", std::move(icon_errors));
  }
  CheckForMigrationAndGatherInfo(FinalizeMode::kFallbackWebAppInfo);
}

void InstallFromSyncCommand::CheckForMigrationAndGatherInfo(FinalizeMode mode) {
  if (!source_app_id_.has_value()) {
    FinalizeInstall(mode);
    return;
  }

  gather_migration_source_info_job_ =
      std::make_unique<GatherMigrationSourceInfoJob>(
          *lock_, *source_app_id_, params_.app_id,
          base::BindOnce(&InstallFromSyncCommand::OnMigrationSourceInfoGathered,
                         weak_ptr_factory_.GetWeakPtr(), mode));
  gather_migration_source_info_job_->Start();
}

void InstallFromSyncCommand::OnMigrationSourceInfoGathered(
    FinalizeMode mode,
    std::optional<GatherMigrationSourceInfoJobResult> migration_source_info) {
  if (migration_source_info) {
    GetMutableDebugValue().Set("migration_source_info",
                               migration_source_info->ToDebugValue());
  }
  migration_source_info_ = std::move(migration_source_info);
  FinalizeInstall(mode);
}

void InstallFromSyncCommand::FinalizeInstall(FinalizeMode mode) {
  WebAppInstallInfo* current_info = mode == FinalizeMode::kNormalWebAppInfo
                                        ? install_info_.get()
                                        : fallback_install_info_.get();
  current_info->generated_icon_fix =
      generated_icon_fix_util::CreateInitialTimeWindow(
          proto::GENERATED_ICON_FIX_SOURCE_SYNC_INSTALL);

  FinalizeJobOptions finalize_options = GetFinalizerOptionForSyncInstall();

  if (migration_source_info_) {
    finalize_options.install_state = migration_source_info_->install_state;
    finalize_options.add_to_desktop =
        migration_source_info_->shortcut_locations.on_desktop;
    finalize_options.add_to_quick_launch_bar =
        migration_source_info_->shortcut_locations.in_quick_launch_bar;
    if (migration_source_info_->run_on_os_login_mode !=
        RunOnOsLoginMode::kNotRun) {
      finalize_options.run_on_os_login_mode =
          migration_source_info_->run_on_os_login_mode;
    }

    current_info->user_display_mode = migration_source_info_->user_display_mode;
  }

  install_job_ = std::make_unique<FinalizeInstallJob>(
      *profile_, lock_.get(), lock_.get(), *current_info, finalize_options);

  install_job_->Start(
      base::BindOnce(&InstallFromSyncCommand::OnInstallFinalized,
                     weak_ptr_factory_.GetWeakPtr(), mode));
}

void InstallFromSyncCommand::OnInstallFinalized(
    FinalizeMode mode,
    const webapps::AppId& app_id,
    webapps::InstallResultCode code) {
  install_job_.reset();
  if (mode == FinalizeMode::kNormalWebAppInfo && !IsSuccess(code)) {
    InstallFallback(code);
    return;
  }

  ReportResultAndDestroy(app_id, code);
}

void InstallFromSyncCommand::InstallFallback(webapps::InstallResultCode code) {
  DCHECK(!IsSuccess(code));
  DCHECK(code != webapps::InstallResultCode::kWebContentsDestroyed);
  DCHECK(code != webapps::InstallResultCode::kInstallTaskDestroyed);
  GetMutableDebugValue().Set("fallback_install", true);
  GetMutableDebugValue().Set("fallback_install_reason", base::ToString(code));

  IconUrlSizeSet icon_urls =
      GetValidIconUrlsToDownload(*fallback_install_info_);

  base::UmaHistogramEnumeration("WebApp.Install.SyncFallbackInstallInitiated",
                                code);

  if (fallback_triggered_for_testing_) {
    std::move(fallback_triggered_for_testing_).Run(code);
  }

  // It is OK to skip downloading the page favicons as everything in is the URL
  // list.
  // TODO(dmurph): Also use favicons. https://crbug.com/1328977.
  data_retriever_->GetIcons(
      &lock_->shared_web_contents(), std::move(icon_urls),
      /*download_page_favicons=*/false,
      /*fail_all_if_any_fail=*/false,
      base::BindOnce(&InstallFromSyncCommand::OnIconsRetrievedForFallbackInfo,
                     weak_ptr_factory_.GetWeakPtr()));
}

void InstallFromSyncCommand::ReportResultAndDestroy(
    const webapps::AppId& app_id,
    webapps::InstallResultCode code) {
  bool success = IsSuccess(code);
  GetMutableDebugValue().Set("result_code", base::ToString(code));
  if (success) {
    if (source_app_id_.has_value()) {
      lock_->install_manager().NotifyWebAppMigrated(*source_app_id_, app_id);
    }
    RecordWebAppInstallationTimestamp(profile_->GetPrefs(), app_id,
                                      webapps::WebappInstallSource::SYNC);
  }
  // The install event is NOT reported on purpose (e.g.
  // `webapps::InstallableMetrics::TrackInstallEvent(source)` is not called), as
  // a sync install is not a recordable install source.
  DCHECK(!webapps::InstallableMetrics::IsReportableInstallSource(
      webapps::WebappInstallSource::SYNC));

  base::UmaHistogramEnumeration("Webapp.InstallResult.Sync", code);
  CompleteAndSelfDestruct(
      success ? CommandResult::kSuccess : CommandResult::kFailure, app_id,
      code);
}

}  // namespace web_app
