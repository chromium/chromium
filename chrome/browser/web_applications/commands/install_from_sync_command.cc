// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/install_from_sync_command.h"

#include <memory>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/install_bounce_metric.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_with_app_lock.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_data_retriever.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/browser/web_contents.h"

namespace web_app {

namespace {

WebAppInstallFinalizer::FinalizeOptions GetFinalizerOptionForSyncInstall() {
  WebAppInstallFinalizer::FinalizeOptions finalize_options(
      webapps::WebappInstallSource::SYNC);
  finalize_options.overwrite_existing_manifest_fields = true;
  // If app is not locally installed then no OS integration like OS shortcuts.
  finalize_options.locally_installed = AreAppsLocallyInstalledBySync();
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

}  // namespace

InstallFromSyncCommand::Params::~Params() = default;

InstallFromSyncCommand::Params::Params(
    AppId app_id,
    const absl::optional<std::string>& manifest_id,
    const GURL& start_url,
    const std::string& title,
    const GURL& scope,
    const absl::optional<SkColor>& theme_color,
    const absl::optional<mojom::UserDisplayMode>& user_display_mode,
    const std::vector<apps::IconInfo>& icons)
    : app_id(app_id),
      manifest_id(manifest_id),
      start_url(start_url),
      title(title),
      scope(scope),
      theme_color(theme_color),
      user_display_mode(user_display_mode),
      icons(icons) {}

InstallFromSyncCommand::Params::Params(const Params&) = default;

InstallFromSyncCommand::InstallFromSyncCommand(
    WebAppUrlLoader* url_loader,
    Profile* profile,
    std::unique_ptr<WebAppDataRetriever> data_retriever,
    const Params& params,
    OnceInstallCallback install_callback)
    : WebAppCommandTemplate<SharedWebContentsWithAppLock>(
          "InstallFromSyncCommand"),
      lock_description_(
          std::make_unique<SharedWebContentsWithAppLockDescription,
                           base::flat_set<AppId>>({params.app_id})),
      url_loader_(url_loader),
      profile_(profile),
      data_retriever_(std::move(data_retriever)),
      params_(params),
      install_callback_(std::move(install_callback)),
      install_error_log_entry_(true, webapps::WebappInstallSource::SYNC) {
#if BUILDFLAG(IS_CHROMEOS)
  DCHECK(AreAppsLocallyInstalledBySync());
#endif
  DCHECK(params_.start_url.is_valid());
  fallback_install_info_ = std::make_unique<WebAppInstallInfo>();
  fallback_install_info_->manifest_id = params_.manifest_id;
  fallback_install_info_->start_url = params_.start_url;
  fallback_install_info_->title = base::UTF8ToUTF16(params_.title);
  fallback_install_info_->user_display_mode = params_.user_display_mode;
  fallback_install_info_->scope = params_.scope;
  fallback_install_info_->theme_color = params_.theme_color;
  fallback_install_info_->manifest_icons = params_.icons;
  debug_value_.Set("app_id", params_.app_id);
  debug_value_.Set("manifest_id", params_.manifest_id.value_or("<unset>"));
  debug_value_.Set("title", params_.title);
  debug_value_.Set(
      "user_display_mode",
      params_.user_display_mode
          ? base::StreamableToString(params_.user_display_mode.value())
          : "<unset>");
  debug_value_.Set("scope", params_.scope.spec());
  debug_value_.Set("start_url", params_.start_url.spec());
  debug_value_.Set("fallback_install", false);
}

InstallFromSyncCommand::~InstallFromSyncCommand() = default;

base::Value InstallFromSyncCommand::ToDebugValue() const {
  base::Value::Dict value = debug_value_.Clone();
  value.Set("error_log", base::Value(error_log_.Clone()));
  return base::Value(std::move(value));
}

void InstallFromSyncCommand::OnShutdown() {
  ReportResultAndDestroy(
      params_.app_id,
      webapps::InstallResultCode::kCancelledOnWebAppProviderShuttingDown);
}

void InstallFromSyncCommand::OnSyncSourceRemoved() {
  // Since this is a sync install command, if an uninstall is queued, just
  // cancel this command.
  ReportResultAndDestroy(params_.app_id,
                         webapps::InstallResultCode::kHaltedBySyncUninstall);
}

const LockDescription& InstallFromSyncCommand::lock_description() const {
  return *lock_description_;
}

void InstallFromSyncCommand::StartWithLock(
    std::unique_ptr<SharedWebContentsWithAppLock> lock) {
  lock_ = std::move(lock);

  url_loader_->LoadUrl(
      params_.start_url, &lock_->shared_web_contents(),
      WebAppUrlLoader::UrlComparison::kIgnoreQueryParamsAndRef,
      base::BindOnce(
          &InstallFromSyncCommand::OnWebAppUrlLoadedGetWebAppInstallInfo,
          weak_ptr_factory_.GetWeakPtr()));
}

void InstallFromSyncCommand::SetFallbackTriggeredForTesting(
    base::OnceCallback<void(webapps::InstallResultCode code)> callback) {
  fallback_triggered_for_testing_ = std::move(callback);
}

void InstallFromSyncCommand::OnWebAppUrlLoadedGetWebAppInstallInfo(
    WebAppUrlLoader::Result result) {
  if (result != WebAppUrlLoader::Result::kUrlLoaded) {
    base::Value::Dict url_loader_error;
    url_loader_error.Set("WebAppUrlLoader::Result",
                         ConvertUrlLoaderResultToString(result));
    error_log_.Append(std::move(url_loader_error));
    install_error_log_entry_.LogUrlLoaderError(
        "OnWebAppUrlLoaded", params_.start_url.spec(), result);
  }

  if (result == WebAppUrlLoader::Result::kRedirectedUrlLoaded) {
    InstallFallback(webapps::InstallResultCode::kInstallURLRedirected);
    return;
  }

  if (result == WebAppUrlLoader::Result::kFailedPageTookTooLong) {
    InstallFallback(webapps::InstallResultCode::kInstallURLLoadTimeOut);
    return;
  }

  if (result != WebAppUrlLoader::Result::kUrlLoaded) {
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
  DCHECK(!install_info_);
  install_info_ = std::move(web_app_info);
  install_info_->user_display_mode = params_.user_display_mode;
  // Prefer the synced title to the one from the page's metadata
  install_info_->title = base::UTF8ToUTF16(params_.title);

  // Populate fallback info with the data retrieved from `GetWebAppInstallInfo`
  fallback_install_info_->description = install_info_->description;
  if (!install_info_->manifest_icons.empty())
    fallback_install_info_->manifest_icons = install_info_->manifest_icons;
  fallback_install_info_->mobile_capable = install_info_->mobile_capable;

  data_retriever_->CheckInstallabilityAndRetrieveManifest(
      &lock_->shared_web_contents(), /*bypass_service_worker_check=*/true,
      base::BindOnce(&InstallFromSyncCommand::OnDidPerformInstallableCheck,
                     weak_ptr_factory_.GetWeakPtr()));
}

void InstallFromSyncCommand::OnDidPerformInstallableCheck(
    blink::mojom::ManifestPtr opt_manifest,
    const GURL& manifest_url,
    bool valid_manifest_for_web_app,
    webapps::InstallableStatusCode error_code) {
  if (opt_manifest) {
    UpdateWebAppInfoFromManifest(*opt_manifest, manifest_url,
                                 install_info_.get());
  } else {
    // If there is no manifest, set the manifest id from the parameters.
    install_info_->manifest_id = params_.manifest_id;
  }

  // Ensure that the manifest linked is the right one.
  AppId generated_app_id =
      GenerateAppId(install_info_->manifest_id, install_info_->start_url);
  if (params_.app_id != generated_app_id) {
    // Add the error to the log.
    base::Value::Dict expected_id_error;
    expected_id_error.Set("expected_app_id", params_.app_id);
    expected_id_error.Set("app_id", generated_app_id);
    error_log_.Append(std::move(expected_id_error));

    install_error_log_entry_.LogExpectedAppIdError(
        "OnDidPerformInstallableCheck", params_.start_url.spec(),
        generated_app_id, params_.app_id);

    InstallFallback(webapps::InstallResultCode::kExpectedAppIdCheckFailed);
    return;
  }

  // If the page doesn't have a favicon, then the icon fetcher will hang
  // forever.
  // TODO(https://crbug.com/1328977): Allow favicons without waiting for them to
  // be updated on the page.
  base::flat_set<GURL> icon_urls = GetValidIconUrlsToDownload(*install_info_);
  data_retriever_->GetIcons(
      &lock_->shared_web_contents(), std::move(icon_urls),
      /*skip_page_favicons=*/true,
      base::BindOnce(&InstallFromSyncCommand::OnIconsRetrievedFinalizeInstall,
                     weak_ptr_factory_.GetWeakPtr(),
                     FinalizeMode::kNormalWebAppInfo));
}

void InstallFromSyncCommand::OnIconsRetrievedFinalizeInstall(
    FinalizeMode mode,
    IconsDownloadedResult result,
    IconsMap icons_map,
    DownloadedIconsHttpResults icons_http_results) {
  WebAppInstallInfo* current_info = mode == FinalizeMode::kNormalWebAppInfo
                                        ? install_info_.get()
                                        : fallback_install_info_.get();
  PopulateProductIcons(current_info, &icons_map);
  PopulateOtherIcons(current_info, icons_map);

  RecordDownloadedIconsHttpResultsCodeClass(
      "WebApp.Icon.HttpStatusCodeClassOnSync", result, icons_http_results);
  UMA_HISTOGRAM_ENUMERATION("WebApp.Icon.DownloadedResultOnSync", result);
  RecordDownloadedIconHttpStatusCodes(
      "WebApp.Icon.DownloadedHttpStatusCodeOnSync", icons_http_results);
  install_error_log_entry_.LogDownloadedIconsErrors(
      *current_info, result, icons_map, icons_http_results);

  lock_->install_finalizer().FinalizeInstall(
      *current_info, GetFinalizerOptionForSyncInstall(),
      base::BindOnce(&InstallFromSyncCommand::OnInstallFinalized,
                     weak_ptr_factory_.GetWeakPtr(), mode));
}

void InstallFromSyncCommand::OnInstallFinalized(FinalizeMode mode,
                                                const AppId& app_id,
                                                webapps::InstallResultCode code,
                                                OsHooksErrors os_hooks_errors) {
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
  debug_value_.Set("fallback_install", true);
  debug_value_.Set("fallback_install_reason", base::StreamableToString(code));

  base::flat_set<GURL> icon_urls =
      GetValidIconUrlsToDownload(*fallback_install_info_);

  base::UmaHistogramEnumeration("WebApp.Install.SyncFallbackInstallInitiated",
                                code);

  if (fallback_triggered_for_testing_)
    std::move(fallback_triggered_for_testing_).Run(code);

  // It is OK to skip downloading the page favicons as everything in is the URL
  // list.
  // TODO(dmurph): Also use favicons. https://crbug.com/1328977.
  data_retriever_->GetIcons(
      &lock_->shared_web_contents(), std::move(icon_urls),
      /*skip_page_favicons=*/true,
      base::BindOnce(&InstallFromSyncCommand::OnIconsRetrievedFinalizeInstall,
                     weak_ptr_factory_.GetWeakPtr(),
                     FinalizeMode::kFallbackWebAppInfo));
}

void InstallFromSyncCommand::ReportResultAndDestroy(
    const AppId& app_id,
    webapps::InstallResultCode code) {
  bool success = IsSuccess(code);
  debug_value_.Set("result_code", base::StreamableToString(code));
  if (success) {
    RecordWebAppInstallationTimestamp(profile_->GetPrefs(), app_id,
                                      webapps::WebappInstallSource::SYNC);
  }
  // The install event is NOT reported on purpose (e.g.
  // `webapps::InstallableMetrics::TrackInstallEvent(source)` is not called), as
  // a sync install is not a recordable install source.
  DCHECK(!webapps::InstallableMetrics::IsReportableInstallSource(
      webapps::WebappInstallSource::SYNC));
  // TODO(https://crbug.com/1303949): migrate LogToInstallManager to take a
  // base::Value::Dict
  if (install_error_log_entry_.HasErrorDict()) {
    command_manager()->LogToInstallManager(
        install_error_log_entry_.TakeErrorDict());
  }

  base::UmaHistogramEnumeration("WebApp.InstallResult.Sync", code);
  SignalCompletionAndSelfDestruct(
      success ? CommandResult::kSuccess : CommandResult::kFailure,
      base::BindOnce(std::move(install_callback_), app_id, code));
}

}  // namespace web_app
