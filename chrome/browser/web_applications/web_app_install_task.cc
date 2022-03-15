// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/web_applications/install_bounce_metric.h"
#include "chrome/browser/web_applications/web_app_data_retriever.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_generator.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_task.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_installation_utils.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_system_web_app_data.h"
#include "chrome/browser/web_applications/web_app_url_loader.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
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
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "net/base/url_util.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/arc.mojom.h"
#include "chromeos/crosapi/mojom/web_app_service.mojom.h"
#include "chromeos/lacros/lacros_service.h"
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

bool IsEmptyIconBitmapsForIconUrl(const IconsMap& icons_map,
                                  const GURL& icon_url) {
  IconsMap::const_iterator iter = icons_map.find(icon_url);
  if (iter == icons_map.end())
    return true;

  const std::vector<SkBitmap>& icon_bitmaps = iter->second;
  if (icon_bitmaps.empty())
    return true;

  for (const SkBitmap& icon_bitmap : icon_bitmaps) {
    if (!icon_bitmap.isNull() && !icon_bitmap.drawsNothing())
      return false;
  }

  return true;
}

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
         lacros_service->init_params()->web_apps_enabled &&
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
    WebAppInstallManager* install_manager,
    WebAppInstallFinalizer* install_finalizer,
    std::unique_ptr<WebAppDataRetriever> data_retriever,
    WebAppRegistrar* registrar)
    : data_retriever_(std::move(data_retriever)),
      install_manager_(install_manager),
      install_finalizer_(install_finalizer),
      profile_(profile),
      registrar_(registrar) {
  if (base::FeatureList::IsEnabled(features::kRecordWebAppDebugInfo))
    error_dict_ = std::make_unique<base::Value>(base::Value::Type::DICTIONARY);
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

void WebAppInstallTask::SetInstallParams(
    const WebAppInstallParams& install_params) {
  if (!install_params.locally_installed) {
    DCHECK(!install_params.add_to_applications_menu);
    DCHECK(!install_params.add_to_desktop);
    DCHECK(!install_params.add_to_quick_launch_bar);
  }
  install_params_ = install_params;
}

void WebAppInstallTask::LoadWebAppAndCheckManifest(
    const GURL& url,
    webapps::WebappInstallSource install_source,
    WebAppUrlLoader* url_loader,
    LoadWebAppAndCheckManifestCallback callback) {
  DCHECK(url_loader);
  CheckInstallPreconditions();
  // Create a WebContents instead of reusing a shared one because we will pass
  // it back to be used for opening the web app.
  // TODO(loyso): Implement stealing of shared web_contents in upcoming
  // WebContentsManager.
  std::unique_ptr<content::WebContents> web_contents =
      CreateWebContents(profile_);

  // Grab WebContents pointer now, before the call to BindOnce might null out
  // |web_contents|.
  content::WebContents* web_contents_ptr = web_contents.get();

  Observe(web_contents.get());
  background_installation_ = false;
  install_callback_ =
      base::BindOnce(std::move(callback), std::move(web_contents));
  install_source_ = install_source;

  url_loader->LoadUrl(
      url, web_contents_ptr,
      WebAppUrlLoader::UrlComparison::kIgnoreQueryParamsAndRef,
      base::BindOnce(
          &WebAppInstallTask::OnWebAppUrlLoadedCheckAndRetrieveManifest,
          GetWeakPtr(), url, web_contents_ptr));
}

void WebAppInstallTask::InstallWebAppFromManifest(
    content::WebContents* contents,
    bool bypass_service_worker_check,
    webapps::WebappInstallSource install_source,
    WebAppInstallDialogCallback dialog_callback,
    OnceInstallCallback install_callback) {
  DCHECK(AreWebAppsUserInstallable(profile_));
  CheckInstallPreconditions();

  Observe(contents);
  dialog_callback_ = std::move(dialog_callback);
  install_callback_ = std::move(install_callback);
  install_source_ = install_source;

  auto web_app_info = std::make_unique<WebAppInstallInfo>();

  if (install_params_)
    ApplyParamsToWebAppInstallInfo(*install_params_, *web_app_info);

  data_retriever_->CheckInstallabilityAndRetrieveManifest(
      web_contents(), bypass_service_worker_check,
      base::BindOnce(&WebAppInstallTask::OnDidPerformInstallableCheck,
                     GetWeakPtr(), std::move(web_app_info),
                     /*force_shortcut_app=*/false));
}

void WebAppInstallTask::InstallWebAppFromManifestWithFallback(
    content::WebContents* contents,
    bool force_shortcut_app,
    webapps::WebappInstallSource install_source,
    WebAppInstallDialogCallback dialog_callback,
    OnceInstallCallback install_callback) {
  DCHECK(AreWebAppsUserInstallable(profile_));
  CheckInstallPreconditions();

  Observe(contents);
  dialog_callback_ = std::move(dialog_callback);
  install_callback_ = std::move(install_callback);
  install_source_ = install_source;

  data_retriever_->GetWebAppInstallInfo(
      web_contents(), base::BindOnce(&WebAppInstallTask::OnGetWebAppInstallInfo,
                                     GetWeakPtr(), force_shortcut_app));
}

void WebAppInstallTask::LoadAndInstallWebAppFromManifestWithFallback(
    const GURL& launch_url,
    content::WebContents* contents,
    WebAppUrlLoader* url_loader,
    webapps::WebappInstallSource install_source,
    OnceInstallCallback install_callback) {
  DCHECK(AreWebAppsUserInstallable(profile_));
  CheckInstallPreconditions();

  Observe(contents);
  if (ShouldStopInstall())
    return;

  background_installation_ = true;
  install_callback_ = std::move(install_callback);
  install_source_ = install_source;

  url_loader->LoadUrl(
      launch_url, contents,
      WebAppUrlLoader::UrlComparison::kIgnoreQueryParamsAndRef,
      base::BindOnce(&WebAppInstallTask::OnWebAppUrlLoadedGetWebAppInstallInfo,
                     GetWeakPtr(), launch_url));
}

void WebAppInstallTask::LoadAndInstallSubAppFromURL(
    const GURL& install_url,
    content::WebContents* contents,
    WebAppUrlLoader* url_loader,
    OnceInstallCallback install_callback) {
  DCHECK(AreWebAppsUserInstallable(profile_));
  CheckInstallPreconditions();

  Observe(contents);
  if (ShouldStopInstall())
    return;

  background_installation_ = true;
  install_callback_ = std::move(install_callback);
  install_source_ = webapps::WebappInstallSource::SUB_APP;

  url_loader->LoadUrl(
      install_url, contents,
      WebAppUrlLoader::UrlComparison::kIgnoreQueryParamsAndRef,
      base::BindOnce(&WebAppInstallTask::OnWebAppUrlLoadedGetWebAppInstallInfo,
                     GetWeakPtr(), install_url));
}

void UpdateFinalizerClientData(
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
    if (params->system_app_type.has_value()) {
      options->system_web_app_data.emplace();
      options->system_web_app_data->system_app_type =
          params->system_app_type.value();
    }
  }
}

void WebAppInstallTask::InstallWebAppFromInfo(
    std::unique_ptr<WebAppInstallInfo> web_application_info,
    bool overwrite_existing_manifest_fields,
    ForInstallableSite for_installable_site,
    webapps::WebappInstallSource install_source,
    OnceInstallCallback callback) {
  CheckInstallPreconditions();

  PopulateProductIcons(web_application_info.get(),
                       /*icons_map*/ nullptr);
  // No IconsMap to populate shortcut item icons from.

  if (install_params_)
    ApplyParamsToWebAppInstallInfo(*install_params_, *web_application_info);

  install_source_ = install_source;
  background_installation_ = true;
  install_callback_ = std::move(callback);

  RecordInstallEvent();

  WebAppInstallFinalizer::FinalizeOptions options;
  options.install_source = install_source;
  options.locally_installed = true;
  options.overwrite_existing_manifest_fields =
      overwrite_existing_manifest_fields;

  UpdateFinalizerClientData(install_params_, &options);
  if (!install_params_) {
    options.bypass_os_hooks = true;
  }

  install_finalizer_->FinalizeInstall(
      *web_application_info, options,
      base::BindOnce(&WebAppInstallTask::OnInstallFinalized, GetWeakPtr()));
}

void WebAppInstallTask::InstallWebAppWithParams(
    content::WebContents* contents,
    const WebAppInstallParams& install_params,
    webapps::WebappInstallSource install_source,
    OnceInstallCallback install_callback) {
  CheckInstallPreconditions();

  Observe(contents);
  SetInstallParams(install_params);
  install_callback_ = std::move(install_callback);
  install_source_ = install_source;
  background_installation_ = true;

  data_retriever_->GetWebAppInstallInfo(
      web_contents(),
      base::BindOnce(&WebAppInstallTask::OnGetWebAppInstallInfo, GetWeakPtr(),
                     /*force_shortcut_app=*/false));
}

void WebAppInstallTask::LoadAndRetrieveWebAppInstallInfoWithIcons(
    const GURL& start_url,
    WebAppUrlLoader* url_loader,
    RetrieveWebAppInstallInfoWithIconsCallback callback) {
  CheckInstallPreconditions();

  retrieve_info_callback_ = std::move(callback);
  background_installation_ = true;
  only_retrieve_web_application_info_ = true;

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

base::Value WebAppInstallTask::TakeErrorDict() {
  DCHECK(error_dict_);
  base::Value error_dict = std::move(*error_dict_);
  error_dict_->DictClear();
  return error_dict;
}

void WebAppInstallTask::SetInstallFinalizerForTesting(
    WebAppInstallFinalizer* install_finalizer) {
  install_finalizer_ = install_finalizer;
}

void WebAppInstallTask::CheckInstallPreconditions() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Concurrent calls are not allowed.
  DCHECK(!web_contents());
  CHECK(!install_callback_);
  CHECK(!retrieve_info_callback_);

  DCHECK(!initiated_);
  initiated_ = true;
}

void WebAppInstallTask::RecordInstallEvent() {
  DCHECK(install_source_ != kNoInstallSource);

  if (webapps::InstallableMetrics::IsReportableInstallSource(install_source_)) {
    webapps::InstallableMetrics::TrackInstallEvent(install_source_);
  }
}

void WebAppInstallTask::CallInstallCallback(const AppId& app_id,
                                            webapps::InstallResultCode code) {
  Observe(nullptr);
  dialog_callback_.Reset();

  install_source_ = kNoInstallSource;

  if (only_retrieve_web_application_info_) {
    DCHECK(retrieve_info_callback_);
    std::move(retrieve_info_callback_).Run(std::move(web_application_info_));
    return;
  }

  DCHECK(install_callback_);
  std::move(install_callback_).Run(app_id, code);
}

bool WebAppInstallTask::ShouldStopInstall() const {
  // Install should stop early if WebContents is being destroyed.
  // WebAppInstallTask::WebContentsDestroyed will get called eventually and
  // the callback will be invoked at that point.
  return !web_contents() || web_contents()->IsBeingDestroyed();
}

void WebAppInstallTask::OnWebAppUrlLoadedGetWebAppInstallInfo(
    const GURL& url_to_load,
    WebAppUrlLoader::Result result) {
  if (ShouldStopInstall())
    return;

  if (result != WebAppUrlLoader::Result::kUrlLoaded)
    LogUrlLoaderError("OnWebAppUrlLoaded", url_to_load.spec(), result);

  if (result == WebAppUrlLoader::Result::kRedirectedUrlLoaded) {
    CallInstallCallback(AppId(),
                        webapps::InstallResultCode::kInstallURLRedirected);
    return;
  }

  if (result == WebAppUrlLoader::Result::kFailedPageTookTooLong) {
    CallInstallCallback(AppId(),
                        webapps::InstallResultCode::kInstallURLLoadTimeOut);
    return;
  }

  if (result != WebAppUrlLoader::Result::kUrlLoaded) {
    CallInstallCallback(AppId(),
                        webapps::InstallResultCode::kInstallURLLoadFailed);
    return;
  }

  data_retriever_->GetWebAppInstallInfo(
      web_contents(),
      base::BindOnce(&WebAppInstallTask::OnGetWebAppInstallInfo, GetWeakPtr(),
                     /*force_shortcut_app*/ false));
}

void WebAppInstallTask::OnWebAppUrlLoadedCheckAndRetrieveManifest(
    const GURL& url_to_load,
    content::WebContents* web_contents,
    WebAppUrlLoader::Result result) {
  if (ShouldStopInstall())
    return;

  if (result != WebAppUrlLoader::Result::kUrlLoaded)
    LogUrlLoaderError("OnWebAppUrlLoaded", url_to_load.spec(), result);

  if (result == WebAppUrlLoader::Result::kRedirectedUrlLoaded) {
    CallInstallCallback(AppId(),
                        webapps::InstallResultCode::kInstallURLRedirected);
    return;
  }

  if (result == WebAppUrlLoader::Result::kFailedPageTookTooLong) {
    CallInstallCallback(AppId(),
                        webapps::InstallResultCode::kInstallURLLoadTimeOut);
    return;
  }

  if (result != WebAppUrlLoader::Result::kUrlLoaded) {
    CallInstallCallback(AppId(),
                        webapps::InstallResultCode::kInstallURLLoadFailed);
    return;
  }

  data_retriever_->CheckInstallabilityAndRetrieveManifest(
      web_contents,
      /*bypass_service_worker_check=*/true,
      base::BindOnce(&WebAppInstallTask::OnWebAppInstallabilityChecked,
                     GetWeakPtr()));
}

void WebAppInstallTask::OnWebAppInstallabilityChecked(
    blink::mojom::ManifestPtr opt_manifest,
    const GURL& manifest_url,
    bool valid_manifest_for_web_app,
    bool is_installable) {
  if (ShouldStopInstall())
    return;

  if (is_installable) {
    DCHECK(opt_manifest);
    CallInstallCallback(GenerateAppIdFromManifest(*opt_manifest),
                        webapps::InstallResultCode::kSuccessNewInstall);
  } else {
    CallInstallCallback(AppId(), webapps::InstallResultCode::kNotInstallable);
  }
}

void WebAppInstallTask::OnGetWebAppInstallInfo(
    bool force_shortcut_app,
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
                     GetWeakPtr(), std::move(web_app_info),
                     force_shortcut_app));
}

void WebAppInstallTask::ApplyParamsToWebAppInstallInfo(
    const WebAppInstallParams& install_params,
    WebAppInstallInfo& web_app_info) {
  if (install_params.user_display_mode != DisplayMode::kUndefined)
    web_app_info.user_display_mode = install_params.user_display_mode;

  if (!install_params.override_manifest_id.has_value())
    web_app_info.manifest_id = install_params.override_manifest_id;

  // If `additional_search_terms` was a manifest property, it would be
  // sanitized while parsing the manifest. Since it's not, we sanitize it
  // here.
  for (const std::string& search_term :
       install_params.additional_search_terms) {
    if (!search_term.empty())
      web_app_info.additional_search_terms.push_back(search_term);
  }

  if (install_params.launch_query_params)
    web_app_info.launch_query_params = install_params.launch_query_params;
}

void WebAppInstallTask::OnDidPerformInstallableCheck(
    std::unique_ptr<WebAppInstallInfo> web_app_info,
    bool force_shortcut_app,
    blink::mojom::ManifestPtr opt_manifest,
    const GURL& manifest_url,
    bool valid_manifest_for_web_app,
    bool is_installable) {
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

  const auto for_installable_site = is_installable && !force_shortcut_app
                                        ? ForInstallableSite::kYes
                                        : ForInstallableSite::kNo;

  if (opt_manifest)
    UpdateWebAppInfoFromManifest(*opt_manifest, manifest_url,
                                 web_app_info.get());

  AppId app_id =
      GenerateAppId(web_app_info->manifest_id, web_app_info->start_url);

  // Does the app_id expectation check if requested.
  if (expected_app_id_.has_value() && *expected_app_id_ != app_id) {
    LogExpectedAppIdError("OnDidPerformInstallableCheck",
                          web_app_info->start_url.spec(), app_id);
    CallInstallCallback(std::move(app_id),
                        webapps::InstallResultCode::kExpectedAppIdCheckFailed);
    return;
  }

  // Duplicate installation check for SUB_APP installs (done here since the
  // AppId isn't available beforehand). It's possible that the app was already
  // installed, but from a different source (eg. by the user manually). In that
  // case we proceed with the installation which adds the SUB_APP install source
  // as well.
  if (install_source_ == webapps::WebappInstallSource::SUB_APP) {
    DCHECK(install_params_ && install_params_->parent_app_id.has_value());
    if (registrar_->WasInstalledBySubApp(app_id)) {
      CallInstallCallback(std::move(app_id),
                          webapps::InstallResultCode::kSuccessAlreadyInstalled);
      return;
    }
  }

  std::vector<GURL> icon_urls = GetValidIconUrlsToDownload(*web_app_info);

  // A system app should always have a manifest icon.
  if (install_source_ == webapps::WebappInstallSource::SYSTEM_DEFAULT) {
    DCHECK(opt_manifest);
    DCHECK(!opt_manifest->icons.empty());
  }

  // If the manifest specified icons, don't use the page icons.
  const bool skip_page_favicons = opt_manifest && !opt_manifest->icons.empty();

  CheckForPlayStoreIntentOrGetIcons(
      std::move(opt_manifest), std::move(web_app_info), std::move(icon_urls),
      for_installable_site, skip_page_favicons);
}

void WebAppInstallTask::CheckForPlayStoreIntentOrGetIcons(
    blink::mojom::ManifestPtr opt_manifest,
    std::unique_ptr<WebAppInstallInfo> web_app_info,
    std::vector<GURL> icon_urls,
    ForInstallableSite for_installable_site,
    bool skip_page_favicons) {
  // Background installations are not a user-triggered installs, and thus
  // cannot be sent to the store.
  if (for_installable_site == ForInstallableSite::kYes &&
      !background_installation_ && opt_manifest) {
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
                             std::move(icon_urls), for_installable_site,
                             skip_page_favicons, intent->intent));
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
                for_installable_site, skip_page_favicons, intent->intent));
        return;
      }
    }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  }
  OnDidCheckForIntentToPlayStore(std::move(web_app_info), std::move(icon_urls),
                                 for_installable_site, skip_page_favicons,
                                 /*intent=*/"",
                                 /*should_intent_to_store=*/false);
}

void WebAppInstallTask::OnDidCheckForIntentToPlayStore(
    std::unique_ptr<WebAppInstallInfo> web_app_info,
    std::vector<GURL> icon_urls,
    ForInstallableSite for_installable_site,
    bool skip_page_favicons,
    const std::string& intent,
    bool should_intent_to_store) {
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
                     GetWeakPtr(), std::move(web_app_info),
                     for_installable_site));
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
void WebAppInstallTask::OnDidCheckForIntentToPlayStoreLacros(
    std::unique_ptr<WebAppInstallInfo> web_app_info,
    std::vector<GURL> icon_urls,
    ForInstallableSite for_installable_site,
    bool skip_page_favicons,
    const std::string& intent,
    crosapi::mojom::IsInstallableResult result) {
  OnDidCheckForIntentToPlayStore(
      std::move(web_app_info), std::move(icon_urls), for_installable_site,
      skip_page_favicons, intent,
      result == crosapi::mojom::IsInstallableResult::kInstallable);
}
#endif

void WebAppInstallTask::InstallWebAppFromInfoRetrieveIcons(
    content::WebContents* web_contents,
    std::unique_ptr<WebAppInstallInfo> web_application_info,
    WebAppInstallFinalizer::FinalizeOptions finalize_options,
    OnceInstallCallback callback) {
  CheckInstallPreconditions();

  Observe(web_contents);
  if (ShouldStopInstall())
    return;

  install_callback_ = std::move(callback);
  install_source_ = finalize_options.install_source;
  background_installation_ = true;

  std::vector<GURL> icon_urls =
      GetValidIconUrlsToDownload(*web_application_info);

  // Skip downloading the page favicons as everything in is the URL list.
  data_retriever_->GetIcons(
      web_contents, std::move(icon_urls),
      /*skip_page_favicons=*/true,
      base::BindOnce(&WebAppInstallTask::OnIconsRetrieved, GetWeakPtr(),
                     std::move(web_application_info),
                     std::move(finalize_options)));
}

void WebAppInstallTask::OnIconsRetrieved(
    std::unique_ptr<WebAppInstallInfo> web_app_info,
    WebAppInstallFinalizer::FinalizeOptions finalize_options,
    IconsDownloadedResult result,
    IconsMap icons_map,
    DownloadedIconsHttpResults icons_http_results) {
  DCHECK(background_installation_);

  if (ShouldStopInstall())
    return;

  DCHECK(web_app_info);

  PopulateProductIcons(web_app_info.get(), &icons_map);
  PopulateOtherIcons(web_app_info.get(), icons_map);

  RecordDownloadedIconsResultAndHttpStatusCodes(result, icons_http_results);
  LogDownloadedIconsErrors(*web_app_info, result, icons_map,
                           icons_http_results);

  install_finalizer_->FinalizeInstall(
      *web_app_info, std::move(finalize_options),
      base::BindOnce(&WebAppInstallTask::OnInstallFinalized, GetWeakPtr()));
}

void WebAppInstallTask::OnIconsRetrievedShowDialog(
    std::unique_ptr<WebAppInstallInfo> web_app_info,
    ForInstallableSite for_installable_site,
    IconsDownloadedResult result,
    IconsMap icons_map,
    DownloadedIconsHttpResults icons_http_results) {
  if (ShouldStopInstall())
    return;

  DCHECK(web_app_info);

  PopulateProductIcons(web_app_info.get(), &icons_map);
  PopulateOtherIcons(web_app_info.get(), icons_map);

  RecordDownloadedIconsResultAndHttpStatusCodes(result, icons_http_results);
  LogDownloadedIconsErrors(*web_app_info, result, icons_map,
                           icons_http_results);

  if (background_installation_) {
    DCHECK(!dialog_callback_);
    OnDialogCompleted(for_installable_site, /*user_accepted=*/true,
                      std::move(web_app_info));
  } else {
    DCHECK(dialog_callback_);
    std::move(dialog_callback_)
        .Run(web_contents(), std::move(web_app_info), for_installable_site,
             base::BindOnce(&WebAppInstallTask::OnDialogCompleted, GetWeakPtr(),
                            for_installable_site));
  }
}

void WebAppInstallTask::OnDialogCompleted(
    ForInstallableSite for_installable_site,
    bool user_accepted,
    std::unique_ptr<WebAppInstallInfo> web_app_info) {
  if (ShouldStopInstall())
    return;

  if (!user_accepted) {
    CallInstallCallback(AppId(),
                        webapps::InstallResultCode::kUserInstallDeclined);
    return;
  }

  if (only_retrieve_web_application_info_) {
    web_application_info_ = std::move(web_app_info);
    CallInstallCallback(AppId(),
                        webapps::InstallResultCode::kSuccessNewInstall);
    return;
  }

  WebAppInstallInfo web_app_info_copy = *web_app_info;

  // This metric is recorded regardless of the installation result.
  RecordInstallEvent();

  WebAppInstallFinalizer::FinalizeOptions finalize_options;
  finalize_options.install_source = install_source_;

  if (install_params_) {
    finalize_options.locally_installed = install_params_->locally_installed;
    finalize_options.overwrite_existing_manifest_fields =
        install_params_->force_reinstall;
    finalize_options.parent_app_id = install_params_->parent_app_id;

    UpdateFinalizerClientData(install_params_, &finalize_options);

    if (install_params_->user_display_mode != DisplayMode::kUndefined)
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
        install_source_ == webapps::WebappInstallSource::SYNC
            ? false
            : kAddAppsToQuickLaunchBarByDefault;
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
                                    install_source_);

  if (install_params_ && !install_params_->locally_installed) {
    DCHECK(background_installation_);
  }

  if (!install_params_ || install_params_->locally_installed) {
    RecordAppBanner(web_contents(), web_app_info->start_url);
  } else {
    DCHECK(background_installation_);
  }

  if (!background_installation_) {
    bool error = os_hooks_errors[OsHookType::kShortcuts];
    const bool can_reparent_tab =
        install_finalizer_->CanReparentTab(app_id, !error);

    if (can_reparent_tab &&
        (web_app_info->user_display_mode != DisplayMode::kBrowser)) {
      install_finalizer_->ReparentTab(app_id, !error, web_contents());
    }
  }
  CallInstallCallback(app_id, webapps::InstallResultCode::kSuccessNewInstall);
}

void WebAppInstallTask::RecordDownloadedIconsResultAndHttpStatusCodes(
    IconsDownloadedResult result,
    const DownloadedIconsHttpResults& icons_http_results) {
  if (install_source_ == webapps::WebappInstallSource::SYNC) {
    RecordDownloadedIconsHttpResultsCodeClass(
        "WebApp.Icon.HttpStatusCodeClassOnSync", result, icons_http_results);

    UMA_HISTOGRAM_ENUMERATION("WebApp.Icon.DownloadedResultOnSync", result);
    RecordDownloadedIconHttpStatusCodes(
        "WebApp.Icon.DownloadedHttpStatusCodeOnSync", icons_http_results);
  } else {
    RecordDownloadedIconsHttpResultsCodeClass(
        "WebApp.Icon.HttpStatusCodeClassOnCreate", result, icons_http_results);

    UMA_HISTOGRAM_ENUMERATION("WebApp.Icon.DownloadedResultOnCreate", result);
    RecordDownloadedIconHttpStatusCodes(
        "WebApp.Icon.DownloadedHttpStatusCodeOnCreate", icons_http_results);
  }
}

void WebAppInstallTask::LogHeaderIfLogEmpty(const std::string& url) {
  if (!error_dict_ || !error_dict_->DictEmpty())
    return;

  error_dict_->SetStringKey("!url", url);
  error_dict_->SetIntKey("install_source", static_cast<int>(install_source_));
  error_dict_->SetBoolKey("background_installation", background_installation_);
  error_dict_->SetKey("stages", base::Value(base::Value::Type::LIST));

  DCHECK(!error_dict_->DictEmpty());
}

void WebAppInstallTask::LogErrorObject(const char* stage,
                                       const std::string& url,
                                       base::Value object) {
  if (!error_dict_)
    return;

  LogHeaderIfLogEmpty(url);

  object.SetStringKey("!stage", stage);
  error_dict_->FindKey("stages")->Append(std::move(object));
}

void WebAppInstallTask::LogUrlLoaderError(const char* stage,
                                          const std::string& url,
                                          WebAppUrlLoader::Result result) {
  if (!error_dict_)
    return;

  base::Value url_loader_error(base::Value::Type::DICTIONARY);

  url_loader_error.SetStringKey("WebAppUrlLoader::Result",
                                ConvertUrlLoaderResultToString(result));

  LogErrorObject(stage, url, std::move(url_loader_error));
}

void WebAppInstallTask::LogExpectedAppIdError(const char* stage,
                                              const std::string& url,
                                              const AppId& app_id) {
  if (!error_dict_ || !expected_app_id_.has_value())
    return;

  base::Value expected_app_id_error(base::Value::Type::DICTIONARY);

  expected_app_id_error.SetStringKey("expected_app_id",
                                     expected_app_id_.value());
  expected_app_id_error.SetStringKey("app_id", app_id);

  LogErrorObject(stage, url, std::move(expected_app_id_error));
}

void WebAppInstallTask::LogDownloadedIconsErrors(
    const WebAppInstallInfo& web_app_info,
    IconsDownloadedResult icons_downloaded_result,
    const IconsMap& icons_map,
    const DownloadedIconsHttpResults& icons_http_results) {
  if (!error_dict_)
    return;

  base::Value icon_errors(base::Value::Type::DICTIONARY);
  {
    // Reports errors only, omits successful entries.
    base::Value icons_http_errors(base::Value::Type::LIST);

    for (const auto& url_and_http_code : icons_http_results) {
      const GURL& icon_url = url_and_http_code.first;
      int http_status_code = url_and_http_code.second;
      const char* http_code_desc = net::GetHttpReasonPhrase(
          static_cast<net::HttpStatusCode>(http_status_code));

      // If the SkBitmap for`icon_url` is missing in `icons_map` then we report
      // this miss as an error, even for net::HttpStatusCode::HTTP_OK.
      if (IsEmptyIconBitmapsForIconUrl(icons_map, icon_url)) {
        base::Value icon_http_error(base::Value::Type::DICTIONARY);

        icon_http_error.SetStringKey("icon_url", icon_url.spec());
        icon_http_error.SetIntKey("http_status_code", http_status_code);
        icon_http_error.SetStringKey("http_code_desc", http_code_desc);

        icons_http_errors.Append(std::move(icon_http_error));
      }
    }

    if (icons_downloaded_result != IconsDownloadedResult::kCompleted ||
        !icons_http_errors.GetListDeprecated().empty()) {
      icon_errors.SetStringKey(
          "icons_downloaded_result",
          IconsDownloadedResultToString(icons_downloaded_result));
    }

    if (!icons_http_errors.GetListDeprecated().empty())
      icon_errors.SetKey("icons_http_results", std::move(icons_http_errors));
  }

  if (web_app_info.is_generated_icon)
    icon_errors.SetBoolKey("is_generated_icon", true);

  if (!icon_errors.DictEmpty()) {
    LogErrorObject("OnIconsRetrieved", web_app_info.start_url.spec(),
                   std::move(icon_errors));
  }
}

}  // namespace web_app
