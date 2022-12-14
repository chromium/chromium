// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/externally_managed_app_install_task.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/web_applications/commands/externally_managed_install_command.h"
#include "chrome/browser/web_applications/commands/install_from_info_command.h"
#include "chrome/browser/web_applications/user_display_mode.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_data_retriever.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_manager.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/installable/installable_params.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

namespace {
// How often we retry to download a custom icon, not counting the first attempt.
const int MAX_ICON_DOWNLOAD_RETRIES = 1;
const base::TimeDelta ICON_DOWNLOAD_RETRY_DELAY = base::Seconds(5);
}  // namespace

namespace web_app {

ExternallyManagedAppInstallTask::ExternallyManagedAppInstallTask(
    Profile* profile,
    WebAppUrlLoader* url_loader,
    WebAppRegistrar* registrar,
    WebAppUiManager* ui_manager,
    WebAppInstallFinalizer* install_finalizer,
    WebAppCommandScheduler* command_scheduler,
    ExternalInstallOptions install_options)
    : profile_(profile),
      url_loader_(url_loader),
      registrar_(registrar),
      install_finalizer_(install_finalizer),
      command_scheduler_(command_scheduler),
      ui_manager_(ui_manager),
      externally_installed_app_prefs_(profile_->GetPrefs()),
      install_options_(std::move(install_options)) {}

ExternallyManagedAppInstallTask::~ExternallyManagedAppInstallTask() = default;

void ExternallyManagedAppInstallTask::Install(
    content::WebContents* web_contents,
    ResultCallback result_callback) {
  if (install_options_.only_use_app_info_factory) {
    DCHECK(install_options_.app_info_factory);
    InstallFromInfo(std::move(result_callback));
    return;
  }

  url_loader_->PrepareForLoad(
      web_contents,
      base::BindOnce(&ExternallyManagedAppInstallTask::OnWebContentsReady,
                     weak_ptr_factory_.GetWeakPtr(), web_contents,
                     std::move(result_callback)));
}

void ExternallyManagedAppInstallTask::SetDataRetrieverFactoryForTesting(
    DataRetrieverFactory data_retriever_factory) {
  data_retriever_factory_ = std::move(data_retriever_factory);
}

void ExternallyManagedAppInstallTask::OnWebContentsReady(
    content::WebContents* web_contents,
    ResultCallback result_callback,
    WebAppUrlLoader::Result prepare_for_load_result) {
  // TODO(crbug.com/1098139): Handle the scenario where WebAppUrlLoader fails to
  // load about:blank and flush WebContents states.
  url_loader_->LoadUrl(
      install_options_.install_url, web_contents,
      WebAppUrlLoader::UrlComparison::kSameOrigin,
      base::BindOnce(&ExternallyManagedAppInstallTask::OnUrlLoaded,
                     weak_ptr_factory_.GetWeakPtr(), web_contents,
                     std::move(result_callback)));
}

void ExternallyManagedAppInstallTask::OnUrlLoaded(
    content::WebContents* web_contents,
    ResultCallback result_callback,
    WebAppUrlLoader::Result load_url_result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(web_contents->GetBrowserContext(), profile_);

  ResultCallback retry_on_failure = base::BindOnce(
      &ExternallyManagedAppInstallTask::TryAppInfoFactoryOnFailure,
      weak_ptr_factory_.GetWeakPtr(), std::move(result_callback));

  if (load_url_result == WebAppUrlLoader::Result::kUrlLoaded) {
    // If we are not re-installing a placeholder, then no need to uninstall
    // anything.
    if (!install_options_.reinstall_placeholder) {
      ContinueWebAppInstall(web_contents, std::move(retry_on_failure));
      return;
    }
    // Calling InstallWebAppWithOptions with the same URL used to install a
    // placeholder won't necessarily replace the placeholder app, because the
    // new app might be installed with a new AppId. To avoid this, always
    // uninstall the placeholder app.
    UninstallPlaceholderApp(web_contents, std::move(retry_on_failure));
    return;
  }

  if (install_options_.install_placeholder) {
    InstallPlaceholder(web_contents, std::move(retry_on_failure));
    return;
  }

  // Avoid counting an error if we are shutting down. This matches later
  // stages of install where if the WebContents is destroyed we return early.
  if (load_url_result == WebAppUrlLoader::Result::kFailedWebContentsDestroyed)
    return;

  webapps::InstallResultCode code =
      webapps::InstallResultCode::kInstallURLLoadFailed;

  switch (load_url_result) {
    case WebAppUrlLoader::Result::kUrlLoaded:
    case WebAppUrlLoader::Result::kFailedWebContentsDestroyed:
      // Handled above.
      NOTREACHED();
      break;
    case WebAppUrlLoader::Result::kRedirectedUrlLoaded:
      code = webapps::InstallResultCode::kInstallURLRedirected;
      break;
    case WebAppUrlLoader::Result::kFailedUnknownReason:
      code = webapps::InstallResultCode::kInstallURLLoadFailed;
      break;
    case WebAppUrlLoader::Result::kFailedPageTookTooLong:
      code = webapps::InstallResultCode::kInstallURLLoadTimeOut;
      break;
    case WebAppUrlLoader::Result::kFailedErrorPageLoaded:
      code = webapps::InstallResultCode::kInstallURLLoadFailed;
      break;
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(retry_on_failure),
                     ExternallyManagedAppManager::InstallResult(code)));
}

void ExternallyManagedAppInstallTask::InstallFromInfo(
    ResultCallback result_callback) {
  auto internal_install_source = ConvertExternalInstallSourceToInstallSource(
      install_options().install_source);
  auto install_params = ConvertExternalInstallOptionsToParams(install_options_);
  install_params.bypass_os_hooks = true;
  auto web_app_info = install_options_.app_info_factory.Run();
  for (std::string& search_term : install_params.additional_search_terms) {
    web_app_info->additional_search_terms.push_back(std::move(search_term));
  }
  web_app_info->install_url = install_params.install_url;
  command_scheduler_->InstallFromInfoWithParams(
      std::move(web_app_info),
      /*overwrite_existing_manifest_fields=*/install_params.force_reinstall,
      internal_install_source,
      base::BindOnce(&ExternallyManagedAppInstallTask::OnWebAppInstalled,
                     weak_ptr_factory_.GetWeakPtr(), /* is_placeholder=*/false,
                     /*offline_install=*/true, std::move(result_callback)),
      install_params);
}

void ExternallyManagedAppInstallTask::UninstallPlaceholderApp(
    content::WebContents* web_contents,
    ResultCallback result_callback) {
  absl::optional<AppId> app_id = registrar_->LookupPlaceholderAppId(
      install_options_.install_url,
      ConvertExternalInstallSourceToSource(install_options_.install_source));

  // If there is no app in the DB that is a placeholder app or if the app exists
  // but is not a placeholder, then no need to uninstall anything.
  // These checks are handled by the WebAppRegistrar itself.
  if (!app_id.has_value()) {
    ContinueWebAppInstall(web_contents, std::move(result_callback));
    return;
  }

  // Otherwise, uninstall the placeholder app.
  install_finalizer_->UninstallExternalWebAppByUrl(
      install_options_.install_url,
      ConvertExternalInstallSourceToSource(install_options_.install_source),
      webapps::WebappUninstallSource::kPlaceholderReplacement,
      base::BindOnce(&ExternallyManagedAppInstallTask::OnPlaceholderUninstalled,
                     weak_ptr_factory_.GetWeakPtr(), web_contents,
                     std::move(result_callback)));
}

void ExternallyManagedAppInstallTask::OnPlaceholderUninstalled(
    content::WebContents* web_contents,
    ResultCallback result_callback,
    webapps::UninstallResultCode code) {
  if (code != webapps::UninstallResultCode::kSuccess) {
    LOG(ERROR) << "Failed to uninstall placeholder for: "
               << install_options_.install_url;
    std::move(result_callback)
        .Run(ExternallyManagedAppManager::InstallResult(
            webapps::InstallResultCode::kFailedPlaceholderUninstall));
    return;
  }
  ContinueWebAppInstall(web_contents, std::move(result_callback));
}

void ExternallyManagedAppInstallTask::ContinueWebAppInstall(
    content::WebContents* web_contents,
    ResultCallback result_callback) {
  if (!data_retriever_factory_) {
    data_retriever_factory_ = base::BindRepeating(
        []() { return std::make_unique<WebAppDataRetriever>(); });
  }

  command_scheduler_->InstallExternallyManagedApp(
      install_options_,
      base::BindOnce(&ExternallyManagedAppInstallTask::OnWebAppInstalled,
                     weak_ptr_factory_.GetWeakPtr(),
                     /*is_placeholder=*/false,
                     /*offline_install=*/false, std::move(result_callback)),
      web_contents->GetWeakPtr(), data_retriever_factory_.Run());
}

void ExternallyManagedAppInstallTask::InstallPlaceholder(
    content::WebContents* web_contents,
    ResultCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  absl::optional<AppId> app_id = registrar_->LookupPlaceholderAppId(
      install_options_.install_url,
      ConvertExternalInstallSourceToSource(install_options_.install_source));

  if (app_id.has_value() && !install_options_.force_reinstall) {
    // No need to install a placeholder app again.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback),
            ExternallyManagedAppManager::InstallResult(
                webapps::InstallResultCode::kSuccessNewInstall, app_id)));
    return;
  }

  if (install_options_.override_icon_url) {
    FetchCustomIcon(install_options_.override_icon_url.value(), web_contents,
                    MAX_ICON_DOWNLOAD_RETRIES, std::move(callback));
    return;
  }

  FinalizePlaceholderInstall(std::move(callback), absl::nullopt);
}

void ExternallyManagedAppInstallTask::FetchCustomIcon(
    const GURL& url,
    content::WebContents* web_contents,
    int retries_left,
    ResultCallback callback) {
  web_contents->DownloadImage(
      url,
      /*is_favicon, whether to not sent/accept cookies*/ true, gfx::Size(),
      /*max_bitmap_size, 0=unlimited*/ 0,
      /*bypass_cache*/ false,
      base::BindOnce(&ExternallyManagedAppInstallTask::OnCustomIconFetched,
                     weak_ptr_factory_.GetWeakPtr(), retries_left, web_contents,
                     std::move(callback)));
}

void ExternallyManagedAppInstallTask::OnCustomIconFetched(
    int retries_left,
    content::WebContents* web_contents,
    ResultCallback callback,
    int id,
    int http_status_code,
    const GURL& image_url,
    const std::vector<SkBitmap>& bitmaps,
    const std::vector<gfx::Size>& sizes) {
  if (bitmaps.size() > 0) {
    // Download succeeded.
    FinalizePlaceholderInstall(std::move(callback), bitmaps);
    return;
  }
  if (retries_left <= 0) {
    // Download failed.
    FinalizePlaceholderInstall(std::move(callback), absl::nullopt);
    return;
  }
  // Retry download.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ExternallyManagedAppInstallTask::FetchCustomIcon,
                     weak_ptr_factory_.GetWeakPtr(), image_url, web_contents,
                     retries_left - 1, std::move(callback)),
      ICON_DOWNLOAD_RETRY_DELAY);
}

void ExternallyManagedAppInstallTask::FinalizePlaceholderInstall(
    ResultCallback callback,
    absl::optional<std::reference_wrapper<const std::vector<SkBitmap>>>
        bitmaps) {
  WebAppInstallInfo web_app_info;

#if defined(CHROMEOS)
  web_app_info.title =
      install_options_.override_name
          ? base::UTF8ToUTF16(install_options_.override_name.value())
          : install_options_.fallback_app_name
                ? base::UTF8ToUTF16(install_options_.fallback_app_name.value())
                : base::UTF8ToUTF16(install_options_.install_url.spec());

  if (bitmaps) {
    IconsMap icons_map;
    icons_map.emplace(GURL(install_options_.override_icon_url.value()),
                      bitmaps.value());
    PopulateProductIcons(&web_app_info, &icons_map);
  }

#else   // defined(CHROMEOS)
  web_app_info.title =
      install_options_.fallback_app_name
          ? base::UTF8ToUTF16(install_options_.fallback_app_name.value())
          : base::UTF8ToUTF16(install_options_.install_url.spec());
#endif  // defined(CHROMEOS)

  web_app_info.start_url = install_options_.install_url;
  web_app_info.install_url = install_options_.install_url;

  web_app_info.user_display_mode = install_options_.user_display_mode;

  WebAppInstallFinalizer::FinalizeOptions options(
      ConvertExternalInstallSourceToInstallSource(
          install_options_.install_source));
  // Overwrite fields if we are doing a forced reinstall, because some
  // values (custom name or icon) might have changed.
  options.overwrite_existing_manifest_fields = install_options_.force_reinstall;

  options.add_to_applications_menu = install_options_.add_to_applications_menu;
  options.add_to_desktop = install_options_.add_to_desktop;
  options.add_to_quick_launch_bar = install_options_.add_to_quick_launch_bar;

  web_app_info.is_placeholder = true;

  install_finalizer_->FinalizeInstall(
      web_app_info, options,
      base::BindOnce(
          &ExternallyManagedAppInstallTask::OnWebAppInstalledWithHooksErrors,
          weak_ptr_factory_.GetWeakPtr(), /*is_placeholder=*/true,
          /*offline_install=*/false, std::move(callback)));
}

void ExternallyManagedAppInstallTask::OnWebAppInstalled(
    bool is_placeholder,
    bool offline_install,
    ResultCallback result_callback,
    const AppId& app_id,
    webapps::InstallResultCode code) {
  OnWebAppInstalledWithHooksErrors(is_placeholder, offline_install,
                                   std::move(result_callback), app_id, code,
                                   OsHooksErrors());
}

void ExternallyManagedAppInstallTask::OnWebAppInstalledWithHooksErrors(
    bool is_placeholder,
    bool offline_install,
    ResultCallback result_callback,
    const AppId& app_id,
    webapps::InstallResultCode code,
    OsHooksErrors os_hooks_errors) {
  if (!IsNewInstall(code)) {
    std::move(result_callback)
        .Run(ExternallyManagedAppManager::InstallResult(code));
    return;
  }

  bool uninstall_and_replace_triggered =
      ui_manager_->UninstallAndReplaceIfExists(
          install_options().uninstall_and_replace, app_id);

  externally_installed_app_prefs_.Insert(install_options_.install_url, app_id,
                                         install_options_.install_source);
  externally_installed_app_prefs_.SetIsPlaceholder(install_options_.install_url,
                                                   is_placeholder);

  if (offline_install) {
    code = install_options().only_use_app_info_factory
               ? webapps::InstallResultCode::kSuccessOfflineOnlyInstall
               : webapps::InstallResultCode::kSuccessOfflineFallbackInstall;
  }

  std::move(result_callback)
      .Run(ExternallyManagedAppManager::InstallResult(
          code, app_id, uninstall_and_replace_triggered));
}

void ExternallyManagedAppInstallTask::TryAppInfoFactoryOnFailure(
    ResultCallback result_callback,
    ExternallyManagedAppManager::InstallResult result) {
  if (!IsSuccess(result.code) && install_options().app_info_factory) {
    InstallFromInfo(std::move(result_callback));
    return;
  }
  std::move(result_callback).Run(std::move(result));
}

}  // namespace web_app
