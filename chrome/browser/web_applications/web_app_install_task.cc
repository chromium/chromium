// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/web_applications/install_bounce_metric.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_generator.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_task.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"
#include "chrome/browser/web_applications/web_contents/web_app_url_loader.h"
#include "chrome/common/chrome_features.h"
#include "components/webapps/browser/features.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "net/http/http_status_code.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/components/arc/mojom/app.mojom.h"
#include "ash/components/arc/mojom/intent_helper.mojom.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/system_web_apps/types/system_web_app_data.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "net/base/url_util.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/arc.mojom.h"
#include "chromeos/crosapi/mojom/web_app_service.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "chromeos/startup/browser_params_proxy.h"
#endif

namespace web_app {

namespace {

#if BUILDFLAG(IS_CHROMEOS)
const char kChromeOsPlayPlatform[] = "chromeos_play";
const char kPlayIntentPrefix[] =
    "https://play.google.com/store/apps/details?id=";
const char kPlayStorePackage[] = "com.android.vending";
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
constexpr bool kAddAppsToQuickLaunchBarByDefault = false;

#else
constexpr bool kAddAppsToQuickLaunchBarByDefault = true;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS)
struct PlayStoreIntent {
  std::string app_id;
  std::string intent;
};

// Find the first Chrome OS app in related_applications of |manifest| and return
// the details necessary to redirect the user to the app's listing in the Play
// Store.
absl::optional<PlayStoreIntent> GetPlayStoreIntentFromManifest(
    const blink::mojom::Manifest& manifest) {
  for (const auto& app : manifest.related_applications) {
    std::string id = base::UTF16ToUTF8(app.id.value_or(std::u16string()));
    if (!base::EqualsASCII(app.platform.value_or(std::u16string()),
                           kChromeOsPlayPlatform)) {
      continue;
    }

    if (id.empty()) {
      // Fallback to ID in the URL.
      if (!net::GetValueForKeyInQuery(app.url, "id", &id) || id.empty()) {
        continue;
      }
    }

    std::string referrer;
    if (net::GetValueForKeyInQuery(app.url, "referrer", &referrer) &&
        !referrer.empty()) {
      referrer = "&referrer=" + referrer;
    }

    std::string intent = kPlayIntentPrefix + id + referrer;
    return PlayStoreIntent{id, intent};
  }
  return absl::nullopt;
}
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
bool ShouldInteractWithArc() {
  auto* lacros_service = chromeos::LacrosService::Get();
  return lacros_service &&
         // Check if the feature is enabled.
         chromeos::BrowserParamsProxy::Get()->WebAppsEnabled() &&
         // Only use ARC installation flow if we know that remote ash-chrome is
         // capable of installing from Play Store in lacros-chrome, to avoid
         // redirecting users to the Play Store if they cannot install
         // anything.
         lacros_service->IsAvailable<crosapi::mojom::WebAppService>();
}

mojo::Remote<crosapi::mojom::Arc>* GetArcRemoteWithMinVersion(
    uint32_t minVersion) {
  auto* lacros_service = chromeos::LacrosService::Get();
  if (lacros_service && lacros_service->IsAvailable<crosapi::mojom::Arc>() &&
      lacros_service->GetInterfaceVersion(crosapi::mojom::Arc::Uuid_) >=
          static_cast<int>(minVersion)) {
    return &lacros_service->GetRemote<crosapi::mojom::Arc>();
  }
  return nullptr;
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

}  // namespace

WebAppInstallTask::WebAppInstallTask(
    Profile* profile,
    WebAppInstallFinalizer* install_finalizer,
    std::unique_ptr<WebAppDataRetriever> data_retriever,
    WebAppRegistrar* registrar,
    webapps::WebappInstallSource install_surface)
    : data_retriever_(std::move(data_retriever)),
      install_finalizer_(install_finalizer),
      profile_(profile),
      registrar_(registrar),
      install_surface_(install_surface),
      log_entry_(/*background_installation=*/false, install_surface) {
  DCHECK_NE(install_surface_, webapps::WebappInstallSource::SYNC);
  // Note: background_installation in the log entry is updated later in the
  // install method calls.
}

WebAppInstallTask::~WebAppInstallTask() {
  // If this task is still observing a WebContents, then the callbacks haven't
  // yet been run.  Run them before the task is destroyed.
  if (web_contents())
    CallInstallCallback(AppId(),
                        webapps::InstallResultCode::kInstallTaskDestroyed);
}

void WebAppInstallTask::ExpectAppId(const AppId& expected_app_id) {
  expected_app_id_ = expected_app_id;
}

void WebAppInstallTask::InstallWebAppFromManifest(
    content::WebContents* contents,
    bool bypass_service_worker_check,
    WebAppInstallDialogCallback dialog_callback,
    OnceInstallCallback install_callback) {
  DCHECK(AreWebAppsUserInstallable(profile_));
  CheckInstallPreconditions();

  Observe(contents);
  dialog_callback_ = std::move(dialog_callback);
  install_callback_ = std::move(install_callback);

  auto web_app_info = std::make_unique<WebAppInstallInfo>();

  if (install_params_)
    ApplyParamsToWebAppInstallInfo(*install_params_, *web_app_info);

  data_retriever_->CheckInstallabilityAndRetrieveManifest(
      web_contents(), bypass_service_worker_check,
      base::BindOnce(&WebAppInstallTask::OnDidPerformInstallableCheck,
                     GetWeakPtr(), std::move(web_app_info)));
}

void WebAppInstallTask::InstallWebAppFromManifestWithFallback(
    content::WebContents* contents,
    WebAppInstallFlow flow,
    WebAppInstallDialogCallback dialog_callback,
    OnceInstallCallback install_callback) {
  DCHECK(AreWebAppsUserInstallable(profile_));
  CheckInstallPreconditions();

  Observe(contents);
  dialog_callback_ = std::move(dialog_callback);
  install_callback_ = std::move(install_callback);
  flow_ = flow;

  data_retriever_->GetWebAppInstallInfo(
      web_contents(),
      base::BindOnce(&WebAppInstallTask::OnGetWebAppInstallInfo, GetWeakPtr()));
}

// static
void WebAppInstallTask::UpdateFinalizerClientData(
    const absl::optional<WebAppInstallParams>& params,
    WebAppInstallFinalizer::FinalizeOptions* options) {
  if (params) {
    if (IsChromeOsDataMandatory()) {
      options->chromeos_data.emplace();
      options->chromeos_data->show_in_launcher =
          params->add_to_applications_menu;
      options->chromeos_data->show_in_search = params->add_to_search;
      options->chromeos_data->show_in_management = params->add_to_management;
      options->chromeos_data->is_disabled = params->is_disabled;
      options->chromeos_data->oem_installed = params->oem_installed;
      options->chromeos_data->handles_file_open_intents =
          params->handles_file_open_intents;
    }
    options->bypass_os_hooks = params->bypass_os_hooks;
    options->add_to_applications_menu = params->add_to_applications_menu;
    options->add_to_desktop = params->add_to_desktop;
    options->add_to_quick_launch_bar = params->add_to_quick_launch_bar;
#if BUILDFLAG(IS_CHROMEOS_ASH)
    if (params->system_app_type.has_value()) {
      options->system_web_app_data.emplace();
      options->system_web_app_data->system_app_type =
          params->system_app_type.value();
    }
#endif
  }
}

void WebAppInstallTask::InstallWebAppFromInfo(
    std::unique_ptr<WebAppInstallInfo> web_app_install_info,
    bool overwrite_existing_manifest_fields,
    OnceInstallCallback callback) {
  CheckInstallPreconditions();

  PopulateProductIcons(web_app_install_info.get(),
                       /*icons_map*/ nullptr);
  // No IconsMap to populate shortcut item icons from.

  if (install_params_)
    ApplyParamsToWebAppInstallInfo(*install_params_, *web_app_install_info);

  background_installation_ = true;
  log_entry_.set_background_installation(true);
  install_callback_ = std::move(callback);

  RecordInstallEvent();

  WebAppInstallFinalizer::FinalizeOptions options(install_surface_);
  options.locally_installed = true;
  options.overwrite_existing_manifest_fields =
      overwrite_existing_manifest_fields;

  if (install_params_) {
    ApplyParamsToFinalizeOptions(*install_params_, options);
  } else {
    options.bypass_os_hooks = true;
  }

  install_finalizer_->FinalizeInstall(
      *web_app_install_info, options,
      base::BindOnce(&WebAppInstallTask::OnInstallFinalized, GetWeakPtr()));
}

void WebAppInstallTask::LoadAndRetrieveWebAppInstallInfoWithIcons(
    const GURL& start_url,
    WebAppUrlLoader* url_loader,
    RetrieveWebAppInstallInfoWithIconsCallback callback) {
  CheckInstallPreconditions();

  retrieve_info_callback_ = std::move(callback);
  background_installation_ = true;
  log_entry_.set_background_installation(true);
  only_retrieve_web_app_install_info_ = true;

  web_contents_ = CreateWebContents(profile_);
  Observe(web_contents_.get());

  DCHECK(url_loader);
  url_loader->LoadUrl(
      start_url, web_contents(),
      WebAppUrlLoader::UrlComparison::kIgnoreQueryParamsAndRef,
      base::BindOnce(&WebAppInstallTask::OnWebAppUrlLoadedGetWebAppInstallInfo,
                     GetWeakPtr(), start_url));
}

// static
std::unique_ptr<content::WebContents> WebAppInstallTask::CreateWebContents(
    Profile* profile) {
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(content::WebContents::CreateParams(profile));

  CreateWebAppInstallTabHelpers(web_contents.get());

  return web_contents;
}

content::WebContents* WebAppInstallTask::GetInstallingWebContents() {
  return web_contents();
}

base::WeakPtr<WebAppInstallTask> WebAppInstallTask::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void WebAppInstallTask::WebContentsDestroyed() {
  CallInstallCallback(AppId(),
                      webapps::InstallResultCode::kWebContentsDestroyed);
}

base::Value::Dict WebAppInstallTask::TakeErrorDict() {
  DCHECK(log_entry_.HasErrorDict());
  return log_entry_.TakeErrorDict();
}

void WebAppInstallTask::CheckInstallPreconditions() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  DCHECK(!profile_->ShutdownStarted());

  // Concurrent calls are not allowed.
  DCHECK(!web_contents());
  CHECK(!install_callback_);
  CHECK(!retrieve_info_callback_);

  DCHECK(!initiated_);
  initiated_ = true;
}

void WebAppInstallTask::RecordInstallEvent() {
  if (webapps::InstallableMetrics::IsReportableInstallSource(
          install_surface_)) {
    webapps::InstallableMetrics::TrackInstallEvent(install_surface_);
  }
}

void WebAppInstallTask::CallInstallCallback(const AppId& app_id,
                                            webapps::InstallResultCode code) {
  Observe(nullptr);
  dialog_callback_.Reset();

  if (only_retrieve_web_app_install_info_) {
    DCHECK(retrieve_info_callback_);
    if (web_app_install_info_) {
      std::move(retrieve_info_callback_).Run(std::move(*web_app_install_info_));
      web_app_install_info_ = absl::nullopt;
    } else {
      std::move(retrieve_info_callback_).Run(code);
    }
    return;
  }

  DCHECK(install_callback_);
  webapps::InstallableMetrics::TrackInstallResult(webapps::IsSuccess(code));
  std::move(install_callback_).Run(app_id, code);
}

bool WebAppInstallTask::ShouldStopInstall() const {
  // Install should stop early if WebContents is being destroyed.
  // WebAppInstallTask::WebContentsDestroyed will get called eventually and
  // the callback will be invoked at that point.
  return !web_contents() || web_contents()->IsBeingDestroyed() ||
         profile_->ShutdownStarted();
}

void WebAppInstallTask::OnWebAppUrlLoadedGetWebAppInstallInfo(
    const GURL& url_to_load,
    WebAppUrlLoader::Result result) {
  if (ShouldStopInstall())
    return;

  if (result != WebAppUrlLoader::Result::kUrlLoaded) {
    log_entry_.LogUrlLoaderError("OnWebAppUrlLoaded", url_to_load.spec(),
                                 result);
  }

  if (result == WebAppUrlLoader::Result::kRedirectedUrlLoaded) {
    CallInstallCallback(expected_app_id_.value_or(AppId()),
                        webapps::InstallResultCode::kInstallURLRedirected);
    return;
  }

  if (result == WebAppUrlLoader::Result::kFailedPageTookTooLong) {
    CallInstallCallback(expected_app_id_.value_or(AppId()),
                        webapps::InstallResultCode::kInstallURLLoadTimeOut);
    return;
  }

  if (result != WebAppUrlLoader::Result::kUrlLoaded) {
    CallInstallCallback(expected_app_id_.value_or(AppId()),
                        webapps::InstallResultCode::kInstallURLLoadFailed);
    return;
  }

  data_retriever_->GetWebAppInstallInfo(
      web_contents(),
      base::BindOnce(&WebAppInstallTask::OnGetWebAppInstallInfo, GetWeakPtr()));
}

void WebAppInstallTask::OnGetWebAppInstallInfo(
    std::unique_ptr<WebAppInstallInfo> web_app_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (ShouldStopInstall())
    return;

  if (!web_app_info) {
    CallInstallCallback(
        AppId(), webapps::InstallResultCode::kGetWebAppInstallInfoFailed);
    return;
  }

  bool bypass_service_worker_check = false;
  if (install_params_) {
    bypass_service_worker_check = install_params_->bypass_service_worker_check;

    // Set start_url to fallback_start_url as web_contents may have been
    // redirected. Will be overridden by manifest values if present.
    DCHECK(install_params_->fallback_start_url.is_valid());
    web_app_info->start_url = install_params_->fallback_start_url;

    if (install_params_->fallback_app_name.has_value())
      web_app_info->title = install_params_->fallback_app_name.value();

    ApplyParamsToWebAppInstallInfo(*install_params_, *web_app_info);
  }

  data_retriever_->CheckInstallabilityAndRetrieveManifest(
      web_contents(), bypass_service_worker_check,
      base::BindOnce(&WebAppInstallTask::OnDidPerformInstallableCheck,
                     GetWeakPtr(), std::move(web_app_info)));
}

void WebAppInstallTask::OnDidPerformInstallableCheck(
    std::unique_ptr<WebAppInstallInfo> web_app_info,
    blink::mojom::ManifestPtr opt_manifest,
    const GURL& manifest_url,
    bool valid_manifest_for_web_app,
    webapps::InstallableStatusCode error_code) {
  if (ShouldStopInstall())
    return;

  DCHECK(web_app_info);

  if (install_params_ && install_params_->require_manifest &&
      !valid_manifest_for_web_app) {
    LOG(WARNING) << "Did not install " << web_app_info->start_url.spec()
                 << " because it didn't have a manifest for web app";
    CallInstallCallback(AppId(),
                        webapps::InstallResultCode::kNotValidManifestForWebApp);
    return;
  }

  if (opt_manifest)
    UpdateWebAppInfoFromManifest(*opt_manifest, manifest_url,
                                 web_app_info.get());

  if (flow_ == WebAppInstallFlow::kCreateShortcut &&
      base::FeatureList::IsEnabled(
          webapps::features::kCreateShortcutIgnoresManifest)) {
    // When creating a shortcut, the |manifest_id| is not part of the App's
    // primary key. The only thing that identifies a shortcut is the start URL,
    // which is always set to the current page.
    *web_app_info = WebAppInstallInfo::CreateInstallInfoForCreateShortcut(
        web_contents()->GetLastCommittedURL(), *web_app_info);
  }

  AppId app_id =
      GenerateAppId(web_app_info->manifest_id, web_app_info->start_url);

  // Does the app_id expectation check if requested.
  if (expected_app_id_.has_value() && *expected_app_id_ != app_id) {
    log_entry_.LogExpectedAppIdError("OnDidPerformInstallableCheck",
                                     web_app_info->start_url.spec(), app_id,
                                     expected_app_id_.value());
    CallInstallCallback(*expected_app_id_,
                        webapps::InstallResultCode::kExpectedAppIdCheckFailed);

    return;
  }

  // Duplicate installation check for SUB_APP installs (done here since the
  // AppId isn't available beforehand). It's possible that the app was already
  // installed, but from a different source (eg. by the user manually). In that
  // case we proceed with the installation which adds the SUB_APP install source
  // as well.
  if (install_surface_ == webapps::WebappInstallSource::SUB_APP) {
    DCHECK(web_app_info->parent_app_id.has_value());
    if (registrar_->WasInstalledBySubApp(app_id)) {
      CallInstallCallback(std::move(app_id),
                          webapps::InstallResultCode::kSuccessAlreadyInstalled);
      return;
    }
  }

  base::flat_set<GURL> icon_urls = GetValidIconUrlsToDownload(*web_app_info);

  // A system app should always have a manifest icon.
  if (install_surface_ == webapps::WebappInstallSource::SYSTEM_DEFAULT) {
    DCHECK(opt_manifest);
    DCHECK(!opt_manifest->icons.empty());
  }

  // If the manifest specified icons, don't use the page icons.
  const bool skip_page_favicons = opt_manifest && !opt_manifest->icons.empty();

  CheckForPlayStoreIntentOrGetIcons(std::move(opt_manifest),
                                    std::move(web_app_info),
                                    std::move(icon_urls), skip_page_favicons);
}

void WebAppInstallTask::CheckForPlayStoreIntentOrGetIcons(
    blink::mojom::ManifestPtr opt_manifest,
    std::unique_ptr<WebAppInstallInfo> web_app_info,
    base::flat_set<GURL> icon_urls,
    bool skip_page_favicons) {
  bool is_create_shortcut = flow_ == WebAppInstallFlow::kCreateShortcut;
  // Background installations are not a user-triggered installs, and thus
  // cannot be sent to the store.
  bool skip_store =
      is_create_shortcut || background_installation_ || !opt_manifest;

  if (!skip_store) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    absl::optional<PlayStoreIntent> intent =
        GetPlayStoreIntentFromManifest(*opt_manifest);
    if (intent) {
      auto* arc_service_manager = arc::ArcServiceManager::Get();
      if (arc_service_manager) {
        auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
            arc_service_manager->arc_bridge_service()->app(), IsInstallable);
        if (instance) {
          instance->IsInstallable(
              intent->app_id,
              base::BindOnce(&WebAppInstallTask::OnDidCheckForIntentToPlayStore,
                             GetWeakPtr(), std::move(web_app_info),
                             std::move(icon_urls), skip_page_favicons,
                             intent->intent));
          return;
        }
      }
    }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
    if (ShouldInteractWithArc()) {
      absl::optional<PlayStoreIntent> intent =
          GetPlayStoreIntentFromManifest(*opt_manifest);
      mojo::Remote<crosapi::mojom::Arc>* opt_arc = GetArcRemoteWithMinVersion(
          crosapi::mojom::Arc::MethodMinVersions::kIsInstallableMinVersion);
      if (opt_arc && intent) {
        mojo::Remote<crosapi::mojom::Arc>& arc = *opt_arc;
        arc->IsInstallable(
            intent->app_id,
            base::BindOnce(
                &WebAppInstallTask::OnDidCheckForIntentToPlayStoreLacros,
                GetWeakPtr(), std::move(web_app_info), std::move(icon_urls),
                skip_page_favicons, intent->intent));
        return;
      }
    }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  }
  OnDidCheckForIntentToPlayStore(std::move(web_app_info), std::move(icon_urls),
                                 skip_page_favicons,
                                 /*intent=*/"",
                                 /*should_intent_to_store=*/false);
}

void WebAppInstallTask::OnDidCheckForIntentToPlayStore(
    std::unique_ptr<WebAppInstallInfo> web_app_info,
    base::flat_set<GURL> icon_urls,
    bool skip_page_favicons,
    const std::string& intent,
    bool should_intent_to_store) {
  if (ShouldStopInstall())
    return;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (should_intent_to_store && !intent.empty()) {
    auto* arc_service_manager = arc::ArcServiceManager::Get();
    if (arc_service_manager) {
      auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
          arc_service_manager->arc_bridge_service()->intent_helper(),
          HandleUrl);
      if (instance) {
        instance->HandleUrl(intent, kPlayStorePackage);
        CallInstallCallback(AppId(),
                            webapps::InstallResultCode::kIntentToPlayStore);
        return;
      }
    }
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (ShouldInteractWithArc() && should_intent_to_store && !intent.empty()) {
    mojo::Remote<crosapi::mojom::Arc>* opt_arc = GetArcRemoteWithMinVersion(
        crosapi::mojom::Arc::MethodMinVersions::kHandleUrlMinVersion);
    if (opt_arc) {
      mojo::Remote<crosapi::mojom::Arc>& arc = *opt_arc;
      arc->HandleUrl(intent, kPlayStorePackage);
      CallInstallCallback(AppId(),
                          webapps::InstallResultCode::kIntentToPlayStore);
      return;
    }
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  data_retriever_->GetIcons(
      web_contents(), std::move(icon_urls), skip_page_favicons,
      base::BindOnce(&WebAppInstallTask::OnIconsRetrievedShowDialog,
                     GetWeakPtr(), std::move(web_app_info)));
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
void WebAppInstallTask::OnDidCheckForIntentToPlayStoreLacros(
    std::unique_ptr<WebAppInstallInfo> web_app_info,
    base::flat_set<GURL> icon_urls,
    bool skip_page_favicons,
    const std::string& intent,
    crosapi::mojom::IsInstallableResult result) {
  OnDidCheckForIntentToPlayStore(
      std::move(web_app_info), std::move(icon_urls), skip_page_favicons, intent,
      result == crosapi::mojom::IsInstallableResult::kInstallable);
}
#endif

void WebAppInstallTask::OnIconsRetrievedShowDialog(
    std::unique_ptr<WebAppInstallInfo> web_app_info,
    IconsDownloadedResult result,
    IconsMap icons_map,
    DownloadedIconsHttpResults icons_http_results) {
  if (ShouldStopInstall())
    return;

  DCHECK(web_app_info);

  PopulateProductIcons(web_app_info.get(), &icons_map);
  PopulateOtherIcons(web_app_info.get(), icons_map);

  RecordDownloadedIconsResultAndHttpStatusCodes(result, icons_http_results);
  log_entry_.LogDownloadedIconsErrors(*web_app_info, result, icons_map,
                                      icons_http_results);

  if (background_installation_) {
    DCHECK(!dialog_callback_);
    OnDialogCompleted(/*user_accepted=*/true, std::move(web_app_info));
  } else {
    DCHECK(dialog_callback_);
    std::move(dialog_callback_)
        .Run(web_contents(), std::move(web_app_info),
             base::BindOnce(&WebAppInstallTask::OnDialogCompleted,
                            GetWeakPtr()));
  }
}

void WebAppInstallTask::OnDialogCompleted(
    bool user_accepted,
    std::unique_ptr<WebAppInstallInfo> web_app_info) {
  if (ShouldStopInstall())
    return;

  if (!user_accepted) {
    CallInstallCallback(AppId(),
                        webapps::InstallResultCode::kUserInstallDeclined);
    return;
  }

  if (only_retrieve_web_app_install_info_) {
    if (web_app_info) {
      web_app_install_info_ = std::move(*web_app_info);
      web_app_info.reset();
    }
    CallInstallCallback(AppId(),
                        webapps::InstallResultCode::kSuccessNewInstall);
    return;
  }

  WebAppInstallInfo web_app_info_copy = web_app_info->Clone();

  // This metric is recorded regardless of the installation result.
  RecordInstallEvent();

  WebAppInstallFinalizer::FinalizeOptions finalize_options(install_surface_);

  if (install_params_) {
    finalize_options.locally_installed = install_params_->locally_installed;
    finalize_options.overwrite_existing_manifest_fields =
        install_params_->force_reinstall;

    ApplyParamsToFinalizeOptions(*install_params_, finalize_options);

    if (install_params_->user_display_mode.has_value())
      web_app_info_copy.user_display_mode = install_params_->user_display_mode;
    finalize_options.add_to_applications_menu =
        install_params_->add_to_applications_menu;
    finalize_options.add_to_desktop = install_params_->add_to_desktop;
    finalize_options.add_to_quick_launch_bar =
        install_params_->add_to_quick_launch_bar;
  } else {
    finalize_options.locally_installed = true;
    finalize_options.overwrite_existing_manifest_fields = true;
    finalize_options.add_to_applications_menu = true;
    finalize_options.add_to_desktop = true;
    finalize_options.add_to_quick_launch_bar =
        kAddAppsToQuickLaunchBarByDefault;
  }

  install_finalizer_->FinalizeInstall(
      web_app_info_copy, finalize_options,
      base::BindOnce(&WebAppInstallTask::OnInstallFinalizedMaybeReparentTab,
                     GetWeakPtr(), std::move(web_app_info)));

  // Check that the finalizer hasn't called OnInstallFinalizedMaybeReparentTab
  // synchronously:
  DCHECK(install_callback_);
}

void WebAppInstallTask::OnInstallFinalized(const AppId& app_id,
                                           webapps::InstallResultCode code,
                                           OsHooksErrors os_hooks_errors) {
  CallInstallCallback(app_id, code);
}

void WebAppInstallTask::OnInstallFinalizedMaybeReparentTab(
    std::unique_ptr<WebAppInstallInfo> web_app_info,
    const AppId& app_id,
    webapps::InstallResultCode code,
    OsHooksErrors os_hooks_errors) {
  if (ShouldStopInstall())
    return;

  if (code != webapps::InstallResultCode::kSuccessNewInstall) {
    CallInstallCallback(app_id, code);
    return;
  }

  RecordWebAppInstallationTimestamp(profile_->GetPrefs(), app_id,
                                    install_surface_);

  if (install_params_ && !install_params_->locally_installed) {
    DCHECK(background_installation_);
  }

  if (!install_params_ || install_params_->locally_installed) {
    RecordAppBanner(web_contents(), web_app_info->start_url);
  } else {
    DCHECK(background_installation_);
  }

  if (!background_installation_ &&
      install_surface_ != webapps::WebappInstallSource::SUB_APP) {
    bool error = os_hooks_errors[OsHookType::kShortcuts];
    const bool can_reparent_tab =
        install_finalizer_->CanReparentTab(app_id, !error);

    if (can_reparent_tab &&
        (web_app_info->user_display_mode != mojom::UserDisplayMode::kBrowser)) {
      install_finalizer_->ReparentTab(app_id, !error, web_contents());
    }
  }
  CallInstallCallback(app_id, webapps::InstallResultCode::kSuccessNewInstall);
}

void WebAppInstallTask::RecordDownloadedIconsResultAndHttpStatusCodes(
    IconsDownloadedResult result,
    const DownloadedIconsHttpResults& icons_http_results) {
  RecordDownloadedIconsHttpResultsCodeClass(
      "WebApp.Icon.HttpStatusCodeClassOnCreate", result, icons_http_results);

  UMA_HISTOGRAM_ENUMERATION("WebApp.Icon.DownloadedResultOnCreate", result);
  RecordDownloadedIconHttpStatusCodes(
      "WebApp.Icon.DownloadedHttpStatusCodeOnCreate", icons_http_results);
}

}  // namespace web_app
