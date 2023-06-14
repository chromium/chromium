// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/fetch_manifest_and_install_command.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/strings/to_string.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/install_bounce_metric.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/locks/noop_lock.h"
#include "chrome/browser/web_applications/locks/web_app_lock_manager.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"
#include "chrome/common/chrome_features.h"
#include "components/webapps/browser/features.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/origin.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/components/arc/mojom/app.mojom.h"
#include "ash/components/arc/mojom/intent_helper.mojom.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "base/strings/string_util.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "base/strings/utf_string_conversions.h"
#include "net/base/url_util.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chromeos/crosapi/mojom/arc.mojom.h"
#include "chromeos/crosapi/mojom/web_app_service.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "chromeos/startup/browser_params_proxy.h"
#endif

namespace web_app {

namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH)
constexpr bool kAddAppsToQuickLaunchBarByDefault = false;
#else
constexpr bool kAddAppsToQuickLaunchBarByDefault = true;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS)
const char kChromeOsPlayPlatform[] = "chromeos_play";
const char kPlayIntentPrefix[] =
    "https://play.google.com/store/apps/details?id=";
const char kPlayStorePackage[] = "com.android.vending";

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
      lacros_service->GetInterfaceVersion<crosapi::mojom::Arc>() >=
          static_cast<int>(minVersion)) {
    return &lacros_service->GetRemote<crosapi::mojom::Arc>();
  }
  return nullptr;
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

}  // namespace

FetchManifestAndInstallCommand::FetchManifestAndInstallCommand(
    webapps::WebappInstallSource install_surface,
    base::WeakPtr<content::WebContents> contents,
    bool bypass_service_worker_check,
    WebAppInstallDialogCallback dialog_callback,
    OnceInstallCallback callback,
    bool use_fallback,
    std::unique_ptr<WebAppDataRetriever> data_retriever)
    : WebAppCommandTemplate<NoopLock>("FetchManifestAndInstallCommand"),
      noop_lock_description_(std::make_unique<NoopLockDescription>()),
      install_surface_(install_surface),
      web_contents_(contents),
      bypass_service_worker_check_(bypass_service_worker_check),
      dialog_callback_(std::move(dialog_callback)),
      install_callback_(std::move(callback)),
      use_fallback_(use_fallback),
      data_retriever_(std::move(data_retriever)),
      install_error_log_entry_(/*background_installation=*/false,
                               install_surface_) {}

FetchManifestAndInstallCommand::~FetchManifestAndInstallCommand() = default;

const LockDescription& FetchManifestAndInstallCommand::lock_description()
    const {
  DCHECK(noop_lock_description_ || app_lock_description_);

  if (app_lock_description_)
    return *app_lock_description_;

  return *noop_lock_description_;
}

void FetchManifestAndInstallCommand::StartWithLock(
    std::unique_ptr<NoopLock> lock) {
  noop_lock_ = std::move(lock);
  if (IsWebContentsDestroyed()) {
    Abort(webapps::InstallResultCode::kWebContentsDestroyed);
    return;
  }

  Observe(web_contents_.get());

  // This metric is recorded regardless of the installation result.
  if (webapps::InstallableMetrics::IsReportableInstallSource(
          install_surface_)) {
    webapps::InstallableMetrics::TrackInstallEvent(install_surface_);
  }

  DCHECK(AreWebAppsUserInstallable(
      Profile::FromBrowserContext(web_contents_->GetBrowserContext())));

  if (use_fallback_) {
    data_retriever_->GetWebAppInstallInfo(
        web_contents_.get(),
        base::BindOnce(&FetchManifestAndInstallCommand::OnGetWebAppInstallInfo,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    FetchManifest();
  }
}

void FetchManifestAndInstallCommand::OnShutdown() {
  Abort(webapps::InstallResultCode::kCancelledOnWebAppProviderShuttingDown);
}

content::WebContents*
FetchManifestAndInstallCommand::GetInstallingWebContents() {
  return web_contents_.get();
}

base::Value FetchManifestAndInstallCommand::ToDebugValue() const {
  auto debug_value = debug_log_.Clone();
  debug_value.Set("app_id", app_id_);
  debug_value.Set("install_surface", static_cast<int>(install_surface_));
  debug_value.Set("used_fallback", use_fallback_);
  return base::Value(std::move(debug_value));
}

void FetchManifestAndInstallCommand::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  if (url::IsSameOriginWith(navigation_handle->GetPreviousPrimaryMainFrameURL(),
                            navigation_handle->GetURL())) {
    return;
  }

  Abort(webapps::InstallResultCode::kCancelledDueToMainFrameNavigation);
}

void FetchManifestAndInstallCommand::Abort(webapps::InstallResultCode code) {
  if (!install_callback_)
    return;
  debug_log_.Set("result_code", base::ToString(code));
  webapps::InstallableMetrics::TrackInstallResult(false);
  Observe(nullptr);
  SignalCompletionAndSelfDestruct(
      CommandResult::kFailure,
      base::BindOnce(std::move(install_callback_), AppId(), code));
}

bool FetchManifestAndInstallCommand::IsWebContentsDestroyed() {
  return !web_contents_ || web_contents_->IsBeingDestroyed();
}

void FetchManifestAndInstallCommand::OnGetWebAppInstallInfo(
    std::unique_ptr<WebAppInstallInfo> fallback_web_app_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (IsWebContentsDestroyed()) {
    Abort(webapps::InstallResultCode::kWebContentsDestroyed);
    return;
  }

  if (!fallback_web_app_info) {
    Abort(webapps::InstallResultCode::kGetWebAppInstallInfoFailed);
    return;
  }

  web_app_info_ = std::move(fallback_web_app_info);
  LogInstallInfo();

  FetchManifest();
}

void FetchManifestAndInstallCommand::FetchManifest() {
  if (IsWebContentsDestroyed()) {
    Abort(webapps::InstallResultCode::kWebContentsDestroyed);
    return;
  }

  data_retriever_->CheckInstallabilityAndRetrieveManifest(
      web_contents_.get(), bypass_service_worker_check_,
      base::BindOnce(
          &FetchManifestAndInstallCommand::OnDidPerformInstallableCheck,
          weak_ptr_factory_.GetWeakPtr()));
}

void FetchManifestAndInstallCommand::OnDidPerformInstallableCheck(
    blink::mojom::ManifestPtr opt_manifest,
    const GURL& manifest_url,
    bool valid_manifest_for_web_app,
    webapps::InstallableStatusCode error_code) {
  if (IsWebContentsDestroyed()) {
    Abort(webapps::InstallResultCode::kWebContentsDestroyed);
    return;
  }

  if (!use_fallback_ && !valid_manifest_for_web_app) {
    LOG(WARNING) << "Did not install " << manifest_url.spec()
                 << " because it didn't have a manifest for web app";
    Abort(webapps::InstallResultCode::kNotValidManifestForWebApp);
    return;
  }
  if (opt_manifest) {
    if (!web_app_info_) {
      web_app_info_ = std::make_unique<WebAppInstallInfo>(opt_manifest->id);
    }
    UpdateWebAppInfoFromManifest(*opt_manifest, manifest_url,
                                 web_app_info_.get());
    LogInstallInfo();
  }

  if (install_surface_ == webapps::WebappInstallSource::MENU_CREATE_SHORTCUT &&
      base::FeatureList::IsEnabled(
          webapps::features::kCreateShortcutIgnoresManifest)) {
    // When creating a shortcut, the |manifest_id| is not part of the App's
    // primary key. The only thing that identifies a shortcut is the start URL,
    // which is always set to the current page.
    *web_app_info_ = WebAppInstallInfo::CreateInstallInfoForCreateShortcut(
        web_contents_->GetLastCommittedURL(), *web_app_info_);
  }

  base::flat_set<GURL> icon_urls = GetValidIconUrlsToDownload(*web_app_info_);

  opt_manifest_ = std::move(opt_manifest);

  // If the manifest specified icons, don't use the page icons.
  const bool skip_page_favicons =
      opt_manifest_ && !opt_manifest_->icons.empty();

  app_id_ = GenerateAppIdFromManifestId(web_app_info_->manifest_id);

  app_lock_description_ =
      command_manager()->lock_manager().UpgradeAndAcquireLock(
          std::move(noop_lock_), {app_id_},
          base::BindOnce(&FetchManifestAndInstallCommand::
                             CheckForPlayStoreIntentOrGetIcons,
                         weak_ptr_factory_.GetWeakPtr(), std::move(icon_urls),
                         skip_page_favicons));
}

void FetchManifestAndInstallCommand::CheckForPlayStoreIntentOrGetIcons(
    base::flat_set<GURL> icon_urls,
    bool skip_page_favicons,
    std::unique_ptr<AppLock> app_lock) {
  app_lock_ = std::move(app_lock);

  bool is_create_shortcut =
      install_surface_ == webapps::WebappInstallSource::MENU_CREATE_SHORTCUT;
  // Background installations are not a user-triggered installs, and thus
  // cannot be sent to the store.
  bool skip_store = is_create_shortcut || !opt_manifest_;

  if (!skip_store) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    absl::optional<PlayStoreIntent> intent =
        GetPlayStoreIntentFromManifest(*opt_manifest_);
    if (intent) {
      auto* arc_service_manager = arc::ArcServiceManager::Get();
      if (arc_service_manager) {
        auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
            arc_service_manager->arc_bridge_service()->app(), IsInstallable);
        if (instance) {
          instance->IsInstallable(
              intent->app_id,
              base::BindOnce(&FetchManifestAndInstallCommand::
                                 OnDidCheckForIntentToPlayStore,
                             weak_ptr_factory_.GetWeakPtr(),
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
          GetPlayStoreIntentFromManifest(*opt_manifest_);
      mojo::Remote<crosapi::mojom::Arc>* opt_arc = GetArcRemoteWithMinVersion(
          crosapi::mojom::Arc::MethodMinVersions::kIsInstallableMinVersion);
      if (opt_arc && intent) {
        mojo::Remote<crosapi::mojom::Arc>& arc = *opt_arc;
        arc->IsInstallable(
            intent->app_id,
            base::BindOnce(&FetchManifestAndInstallCommand::
                               OnDidCheckForIntentToPlayStoreLacros,
                           weak_ptr_factory_.GetWeakPtr(), std::move(icon_urls),
                           skip_page_favicons, intent->intent));
        return;
      }
    }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  }
  OnDidCheckForIntentToPlayStore(std::move(icon_urls), skip_page_favicons,
                                 /*intent=*/"",
                                 /*should_intent_to_store=*/false);
}

void FetchManifestAndInstallCommand::OnDidCheckForIntentToPlayStore(
    base::flat_set<GURL> icon_urls,
    bool skip_page_favicons,
    const std::string& intent,
    bool should_intent_to_store) {
  if (IsWebContentsDestroyed()) {
    Abort(webapps::InstallResultCode::kWebContentsDestroyed);
    return;
  }
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (should_intent_to_store && !intent.empty()) {
    auto* arc_service_manager = arc::ArcServiceManager::Get();
    if (arc_service_manager) {
      auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
          arc_service_manager->arc_bridge_service()->intent_helper(),
          HandleUrl);
      if (instance) {
        instance->HandleUrl(intent, kPlayStorePackage);
        Abort(webapps::InstallResultCode::kIntentToPlayStore);
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
      Abort(webapps::InstallResultCode::kIntentToPlayStore);
      return;
    }
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  data_retriever_->GetIcons(
      web_contents_.get(), std::move(icon_urls), skip_page_favicons,
      base::BindOnce(
          &FetchManifestAndInstallCommand::OnIconsRetrievedShowDialog,
          weak_ptr_factory_.GetWeakPtr()));
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
void FetchManifestAndInstallCommand::OnDidCheckForIntentToPlayStoreLacros(
    base::flat_set<GURL> icon_urls,
    bool skip_page_favicons,
    const std::string& intent,
    crosapi::mojom::IsInstallableResult result) {
  OnDidCheckForIntentToPlayStore(
      std::move(icon_urls), skip_page_favicons, intent,
      result == crosapi::mojom::IsInstallableResult::kInstallable);
}
#endif

void FetchManifestAndInstallCommand::OnIconsRetrievedShowDialog(
    IconsDownloadedResult result,
    IconsMap icons_map,
    DownloadedIconsHttpResults icons_http_results) {
  if (IsWebContentsDestroyed()) {
    Abort(webapps::InstallResultCode::kWebContentsDestroyed);
    return;
  }

  DCHECK(web_app_info_);

  PopulateProductIcons(web_app_info_.get(), &icons_map);
  PopulateOtherIcons(web_app_info_.get(), icons_map);

  RecordDownloadedIconsResultAndHttpStatusCodes(result, icons_http_results);
  install_error_log_entry_.LogDownloadedIconsErrors(
      *web_app_info_, result, icons_map, icons_http_results);

  if (!dialog_callback_) {
    OnDialogCompleted(/*user_accepted=*/true, std::move(web_app_info_));
  } else {
    std::move(dialog_callback_)
        .Run(web_contents_.get(), std::move(web_app_info_),
             base::BindOnce(&FetchManifestAndInstallCommand::OnDialogCompleted,
                            weak_ptr_factory_.GetWeakPtr()));
  }
}

void FetchManifestAndInstallCommand::OnDialogCompleted(
    bool user_accepted,
    std::unique_ptr<WebAppInstallInfo> web_app_info) {
  if (IsWebContentsDestroyed()) {
    Abort(webapps::InstallResultCode::kWebContentsDestroyed);
    return;
  }

  if (!user_accepted) {
    Abort(webapps::InstallResultCode::kUserInstallDeclined);
    return;
  }

  web_app_info_ = std::move(web_app_info);

  WebAppInstallFinalizer::FinalizeOptions finalize_options(install_surface_);

  finalize_options.locally_installed = true;
  finalize_options.overwrite_existing_manifest_fields = true;
  finalize_options.add_to_applications_menu = true;
  finalize_options.add_to_desktop = true;
  finalize_options.add_to_quick_launch_bar = kAddAppsToQuickLaunchBarByDefault;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (ResolveExperimentalWebAppIsolationFeature() ==
      ExperimentalWebAppIsolationMode::kProfile) {
    app_profile_path_ = absl::make_optional(GenerateWebAppProfilePath(app_id_));
    finalize_options.chromeos_data.emplace();
    finalize_options.chromeos_data->app_profile_path = app_profile_path_;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  DCHECK(app_lock_);
  app_lock_->install_finalizer().FinalizeInstall(
      *web_app_info_, finalize_options,
      base::BindOnce(
          &FetchManifestAndInstallCommand::OnInstallFinalizedMaybeReparentTab,
          weak_ptr_factory_.GetWeakPtr()));

  // Check that the finalizer hasn't called OnInstallFinalizedMaybeReparentTab
  // synchronously:
  DCHECK(install_callback_);
}

void FetchManifestAndInstallCommand::OnInstallFinalizedMaybeReparentTab(
    const AppId& app_id,
    webapps::InstallResultCode code,
    OsHooksErrors os_hooks_errors) {
  if (IsWebContentsDestroyed()) {
    Abort(webapps::InstallResultCode::kWebContentsDestroyed);
    return;
  }

  if (code != webapps::InstallResultCode::kSuccessNewInstall) {
    Abort(code);
    return;
  }

  RecordWebAppInstallationTimestamp(
      Profile::FromBrowserContext(web_contents_->GetBrowserContext())
          ->GetPrefs(),
      app_id, install_surface_);

  bool error = os_hooks_errors[OsHookType::kShortcuts];
  DCHECK(app_lock_);
  const bool can_reparent_tab =
      app_lock_->install_finalizer().CanReparentTab(app_id, !error);

  if (can_reparent_tab &&
      (web_app_info_->user_display_mode != mojom::UserDisplayMode::kBrowser)) {
    app_lock_->install_finalizer().ReparentTab(app_id, !error,
                                               web_contents_.get());
  }

  OnInstallCompleted(app_id, webapps::InstallResultCode::kSuccessNewInstall);
}

void FetchManifestAndInstallCommand::OnInstallCompleted(
    const AppId& app_id,
    webapps::InstallResultCode code) {
  if (base::FeatureList::IsEnabled(features::kRecordWebAppDebugInfo)) {
    if (install_error_log_entry_.HasErrorDict()) {
      command_manager()->LogToInstallManager(
          install_error_log_entry_.TakeErrorDict());
    }
  }
  debug_log_.Set("result_code", base::ToString(code));

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // `web_app_info_` might be moved after this point. This is ok since we don't
  // need it here any more.
  if (app_profile_path_) {
    CHECK(ResolveExperimentalWebAppIsolationFeature() ==
          ExperimentalWebAppIsolationMode::kProfile);
    // Create the app profile and install the same app inside it too.
    g_browser_process->profile_manager()->CreateProfileAsync(
        app_profile_path_.value(),
        /*initialized_callback=*/
        base::BindOnce(
            [](std::unique_ptr<WebAppInstallInfo> web_app_info,
               webapps::WebappInstallSource install_surface,
               Profile* app_profile) {
              CHECK(app_profile) << "failed to create app profile";
              auto* provider = WebAppProvider::GetForWebApps(app_profile);
              provider->scheduler().InstallFromInfo(
                  std::move(web_app_info),
                  /*overwrite_existing_manifest_fields=*/true, install_surface,
                  base::DoNothing());
            },
            std::move(web_app_info_), install_surface_),
        /*created_callback=*/base::DoNothing());
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  webapps::InstallableMetrics::TrackInstallResult(webapps::IsSuccess(code));
  SignalCompletionAndSelfDestruct(
      webapps::IsSuccess(code) ? CommandResult::kSuccess
                               : CommandResult::kFailure,
      base::BindOnce(std::move(install_callback_), app_id, code));
}

void FetchManifestAndInstallCommand::LogInstallInfo() {
  debug_log_.Set("manifest_id", web_app_info_->manifest_id.spec());
  debug_log_.Set("start_url", web_app_info_->start_url.spec());
  debug_log_.Set("name", web_app_info_->title);
}
}  // namespace web_app
