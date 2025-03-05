// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/fetch_manifest_and_install_command.h"

#include <memory>
#include <utility>

#include "base/auto_reset.h"
#include "base/check_is_test.h"
#include "base/debug/crash_logging.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/to_string.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/commands/command_metrics.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/install_bounce_metric.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/locks/noop_lock.h"
#include "chrome/browser/web_applications/locks/web_app_lock_manager.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/proto/web_app_os_integration_state.pb.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_operations.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"
#include "chrome/browser/web_applications/web_contents/web_contents_manager.h"
#include "chrome/common/chrome_features.h"
#include "components/webapps/browser/features.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_evaluator.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/installable/installable_params.h"
#include "components/webapps/common/constants.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/manifest_icon_downloader.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/url_constants.h"
#include "url/origin.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ash/experiences/arc/mojom/app.mojom.h"
#include "chromeos/ash/experiences/arc/mojom/intent_helper.mojom.h"
#include "chromeos/ash/experiences/arc/session/arc_bridge_service.h"
#include "chromeos/ash/experiences/arc/session/arc_service_manager.h"
#include "net/base/url_util.h"
#endif

namespace web_app {

namespace {

constexpr bool kAddAppsToQuickLaunchBarByDefault = !BUILDFLAG(IS_CHROMEOS);

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
std::optional<PlayStoreIntent> GetPlayStoreIntentFromManifest(
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
  return std::nullopt;
}
#endif  // BUILDFLAG(IS_CHROMEOS)

void LogInstallInfo(base::Value::Dict& dict,
                    const WebAppInstallInfo& install_info) {
  dict.Set("manifest_id", install_info.manifest_id().spec());
  dict.Set("start_url", install_info.start_url().spec());
  dict.Set("name", install_info.title);
}

bool IsShortcutCreated(WebAppRegistrar& registrar,
                       const webapps::AppId& app_id) {
  auto os_state = registrar.GetAppCurrentOsIntegrationState(app_id);
  if (!os_state.has_value()) {
    return false;
  }

  return os_state->has_shortcut();
}

static bool& ShouldBypassVisibilityChecks() {
  static bool g_bypass_visibility_checking = false;
  return g_bypass_visibility_checking;
}

int ComputeIdealScreenshotSize(
    const blink::mojom::ManifestScreenshotPtr& screenshot) {
  return screenshot->image.sizes.empty()
             ? webapps::kMinimumScreenshotSizeInPx
             : std::max(screenshot->image.sizes[0].width(),
                        screenshot->image.sizes[0].height());
}

bool IsValidScreenshotForDownload(
    const blink::mojom::ManifestScreenshotPtr& screenshot) {
  return screenshot->form_factor ==
         blink::mojom::ManifestScreenshot::FormFactor::kWide;
}

}  // namespace

// static
base::AutoReset<bool>
FetchManifestAndInstallCommand::BypassVisibilityCheckForTesting() {
  CHECK_IS_TEST();
  return base::AutoReset<bool>(&ShouldBypassVisibilityChecks(), true);
}

FetchManifestAndInstallCommand::FetchManifestAndInstallCommand(
    webapps::WebappInstallSource install_surface,
    base::WeakPtr<content::WebContents> contents,
    WebAppInstallDialogCallback dialog_callback,
    OnceInstallCallback callback,
    FallbackBehavior fallback_behavior,
    base::WeakPtr<WebAppUiManager> ui_manager)
    : WebAppCommand<NoopLock,
                    const webapps::AppId&,
                    webapps::InstallResultCode>(
          "FetchManifestAndInstallCommand",
          NoopLockDescription(),
          std::move(callback),
          /*args_for_shutdown=*/
          std::make_tuple(webapps::AppId(),
                          webapps::InstallResultCode::
                              kCancelledOnWebAppProviderShuttingDown)),
      install_surface_(install_surface),
      web_contents_(contents),
      dialog_callback_(std::move(dialog_callback)),
      fallback_behavior_(fallback_behavior),
      ui_manager_(ui_manager),
      install_error_log_entry_(/*background_installation=*/false,
                               install_surface_) {
  Observe(web_contents_.get());
  GetMutableDebugValue().Set("visible_url",
                             web_contents_->GetVisibleURL().spec());
  GetMutableDebugValue().Set("last_committed_url",
                             web_contents_->GetLastCommittedURL().spec());
  GetMutableDebugValue().Set("initial_visibility",
                             static_cast<int>(web_contents()->GetVisibility()));
  GetMutableDebugValue().Set("install_surface",
                             static_cast<int>(install_surface_));
  GetMutableDebugValue().Set("fallback_behavior",
                             base::ToString(fallback_behavior_));
}

FetchManifestAndInstallCommand::~FetchManifestAndInstallCommand() = default;

void FetchManifestAndInstallCommand::OnShutdown(
    base::PassKey<WebAppCommandManager>) const {
  webapps::InstallableMetrics::TrackInstallResult(false, install_surface_);
}

content::WebContents* FetchManifestAndInstallCommand::GetInstallingWebContents(
    base::PassKey<WebAppCommandManager>) {
  return web_contents_.get();
}

void FetchManifestAndInstallCommand::GetScreenshot(
    int index,
    base::OnceCallback<void(SkBitmap, std::optional<std::u16string>)>
        callback) {
  // If the screenshot for a specific index has been downloaded, run the
  // callback instantly.
  if (base::Contains(screenshots_downloaded_, index)) {
    auto screenshot_info = screenshots_downloaded_.at(index);
    std::move(callback).Run(
        std::get<SkBitmap>(screenshot_info),
        std::get<std::optional<std::u16string>>(screenshot_info));
    return;
  }

  // Store pending callbacks to be run later on once the screenshot finishes
  // downloading.
  pending_screenshot_callbacks_.insert_or_assign(index, std::move(callback));
}

const std::vector<gfx::Size>&
FetchManifestAndInstallCommand::GetScreenshotSizes() {
  return screenshot_sizes_;
}

void FetchManifestAndInstallCommand::StartWithLock(
    std::unique_ptr<NoopLock> lock) {
  noop_lock_ = std::move(lock);
  if (IsWebContentsDestroyed()) {
    Abort(webapps::InstallResultCode::kWebContentsDestroyed);
    return;
  }

  if (web_contents()->GetVisibility() != content::Visibility::VISIBLE &&
      !ShouldBypassVisibilityChecks()) {
    Abort(webapps::InstallResultCode::kCancelledDueToMainFrameNavigation);
    return;
  }

  if (did_navigation_occur_before_start_) {
    Abort(webapps::InstallResultCode::kCancelledDueToMainFrameNavigation);
    return;
  }

  // This metric is recorded regardless of the installation result.
  if (webapps::InstallableMetrics::IsReportableInstallSource(
          install_surface_)) {
    webapps::InstallableMetrics::TrackInstallEvent(install_surface_);
  }

  if (!AreWebAppsUserInstallable(
          Profile::FromBrowserContext(web_contents()->GetBrowserContext()))) {
    Abort(webapps::InstallResultCode::kWebAppDisabled);
    return;
  }

  data_retriever_ = noop_lock_->web_contents_manager().CreateDataRetriever();

  switch (fallback_behavior_) {
    case FallbackBehavior::kCraftedManifestOnly:
      FetchManifest();
      return;
    case FallbackBehavior::kUseFallbackInfoWhenNotInstallable:
    case FallbackBehavior::kAllowFallbackDataAlways:
      data_retriever_->GetWebAppInstallInfo(
          web_contents(),
          base::BindOnce(
              &FetchManifestAndInstallCommand::OnGetWebAppInstallInfo,
              weak_ptr_factory_.GetWeakPtr()));
      return;
  }
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

  if (!IsStarted()) {
    did_navigation_occur_before_start_ = true;
    return;
  }

  Abort(webapps::InstallResultCode::kCancelledDueToMainFrameNavigation);
}

void FetchManifestAndInstallCommand::OnVisibilityChanged(
    content::Visibility visibility) {
  if (ShouldBypassVisibilityChecks()) {
    return;
  }
  if (visibility == content::Visibility::VISIBLE) {
    return;
  }

  if (!IsStarted()) {
    did_navigation_occur_before_start_ = true;
    return;
  }

  // This prevents us from closing the dialog if the visibility of the window
  // itself changes but the tab doesn't. A more thorough fix here is to listen
  // to the tab strip changing (AKA a different tab being opened / changed to),
  // but due to needing to use code in `ui`, this is a bit hard to do.
  if (ui_manager_->IsWebContentsActiveTabInBrowser(web_contents())) {
    return;
  }

  Abort(webapps::InstallResultCode::kCancelledDueToMainFrameNavigation);
}

void FetchManifestAndInstallCommand::WebContentsDestroyed() {
  Observe(nullptr);
  // No need to abort - web content destruction is handled in the beginning of
  // each method. However, this needs to be here in case the web contents is
  // destroyed before the command is started.
}

void FetchManifestAndInstallCommand::Abort(webapps::InstallResultCode code,
                                           const base::Location& location) {
  GetMutableDebugValue().Set("result_code", base::ToString(code));
  webapps::InstallableMetrics::TrackInstallResult(false, install_surface_);
  Observe(nullptr);
  MeasureUserInstalledAppHistogram(code);
  CompleteAndSelfDestruct(CommandResult::kFailure, webapps::AppId(), code,
                          location);
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
  LogInstallInfo(*GetMutableDebugValue().EnsureDict("fallback_web_app_info"),
                 *web_app_info_);

  FetchManifest();
}

void FetchManifestAndInstallCommand::FetchManifest() {
  if (IsWebContentsDestroyed()) {
    Abort(webapps::InstallResultCode::kWebContentsDestroyed);
    return;
  }
  webapps::InstallableParams params;
  params.installable_criteria =
      webapps::InstallableCriteria::kValidManifestIgnoreDisplay;
  switch (fallback_behavior_) {
    case FallbackBehavior::kCraftedManifestOnly:
      params.valid_primary_icon = true;
      params.check_eligibility = true;
      break;
    case FallbackBehavior::kAllowFallbackDataAlways:
    case FallbackBehavior::kUseFallbackInfoWhenNotInstallable:
      break;
  }

  data_retriever_->CheckInstallabilityAndRetrieveManifest(
      web_contents_.get(),
      base::BindOnce(
          &FetchManifestAndInstallCommand::OnDidPerformInstallableCheck,
          weak_ptr_factory_.GetWeakPtr()),
      params);
}

void FetchManifestAndInstallCommand::OnDidPerformInstallableCheck(
    blink::mojom::ManifestPtr opt_manifest,
    bool valid_manifest_for_web_app,
    webapps::InstallableStatusCode error_code) {
  valid_manifest_for_crafted_web_app_ = valid_manifest_for_web_app;
  GetMutableDebugValue().Set(
      "manifest_url",
      opt_manifest ? opt_manifest->manifest_url.possibly_invalid_spec() : "");
  GetMutableDebugValue().Set("valid_manifest_for_web_app",
                             valid_manifest_for_web_app);
  GetMutableDebugValue().Set("installable_error_code",
                             base::ToString(error_code));
  if (IsWebContentsDestroyed()) {
    Abort(webapps::InstallResultCode::kWebContentsDestroyed);
    return;
  }
  // A manifest should always be returned unless an irrecoverable error occurs.
  if (!opt_manifest) {
    Abort(webapps::InstallResultCode::kNotInstallable);
    return;
  }

  switch (fallback_behavior_) {
    case FallbackBehavior::kCraftedManifestOnly:
      if (!valid_manifest_for_web_app) {
        LOG(WARNING) << "Did not install "
                     << (opt_manifest->manifest_url.is_valid()
                             ? opt_manifest->manifest_url.spec()
                             : web_contents()->GetLastCommittedURL().spec())
                     << " because it didn't have a manifest for web app";
        Abort(webapps::InstallResultCode::kNotValidManifestForWebApp);
        return;
      }
      web_app_info_ = std::make_unique<WebAppInstallInfo>(
          opt_manifest->id, opt_manifest->start_url);
      break;
    case FallbackBehavior::kUseFallbackInfoWhenNotInstallable: {
      webapps::InstallableStatusCode display_installable =
          webapps::InstallableEvaluator::GetDisplayError(
              *opt_manifest,
              webapps::InstallableCriteria::kValidManifestWithIcons);
      GetMutableDebugValue().Set("display_installable_code",
                                 base::ToString(display_installable));
      // Since the valid_manifest_for_web_app used the
      // `kValidManifestIgnoreDisplay` criteria, add the display check to see if
      // this app was fully promotable/crafted.
      bool promotable = valid_manifest_for_web_app &&
                        display_installable ==
                            webapps::InstallableStatusCode::NO_ERROR_DETECTED;
      // If the manifest is crafted, override the fallback install info.
      if (promotable) {
        web_app_info_ = std::make_unique<WebAppInstallInfo>(
            opt_manifest->id, opt_manifest->start_url);
      } else {
        web_app_info_->is_diy_app = true;
      }
      break;
    }
    case FallbackBehavior::kAllowFallbackDataAlways:
      CHECK(web_app_info_);
      break;
  }
  GetMutableDebugValue().Set("is_diy_app", web_app_info_->is_diy_app);
  CHECK(opt_manifest->start_url.is_valid());
  CHECK(opt_manifest->id.is_valid());
  UpdateWebAppInfoFromManifest(*opt_manifest, web_app_info_.get());
  LogInstallInfo(GetMutableDebugValue(), *web_app_info_);

  icons_from_manifest_ = GetValidIconUrlsToDownload(*web_app_info_);
  for (const IconUrlWithSize& icon_with_size : icons_from_manifest_) {
    GetMutableDebugValue()
        .EnsureList("icon_urls_from_manifest")
        ->Append(icon_with_size.ToString());
  }

  opt_manifest_ = std::move(opt_manifest);
  StartPreloadingScreenshots();

  switch (fallback_behavior_) {
    case FallbackBehavior::kCraftedManifestOnly:
      CHECK(!opt_manifest_->icons.empty())
          << "kValidManifestIgnoreDisplay guarantees a manifest icon.";
      skip_page_favicons_on_initial_download_ = true;
      break;
    case FallbackBehavior::kUseFallbackInfoWhenNotInstallable:
      skip_page_favicons_on_initial_download_ = valid_manifest_for_web_app;
      break;
    case FallbackBehavior::kAllowFallbackDataAlways:
      skip_page_favicons_on_initial_download_ = false;
      break;
  }
  GetMutableDebugValue().Set("skip_page_favicons_on_initial_download",
                             skip_page_favicons_on_initial_download_);

  app_lock_ = std::make_unique<AppLock>();
  command_manager()->lock_manager().UpgradeAndAcquireLock(
      std::move(noop_lock_), *app_lock_,
      {GenerateAppIdFromManifestId(web_app_info_->manifest_id())},
      base::BindOnce(
          &FetchManifestAndInstallCommand::CheckForPlayStoreIntentOrGetIcons,
          weak_ptr_factory_.GetWeakPtr()));
}

void FetchManifestAndInstallCommand::CheckForPlayStoreIntentOrGetIcons() {
  CHECK(app_lock_);
  CHECK(app_lock_->IsGranted());

  bool is_create_shortcut =
      install_surface_ == webapps::WebappInstallSource::MENU_CREATE_SHORTCUT;
  // Background installations are not a user-triggered installs, and thus
  // cannot be sent to the store.
  bool skip_store = is_create_shortcut || !opt_manifest_;

  if (!skip_store) {
#if BUILDFLAG(IS_CHROMEOS)
    std::optional<PlayStoreIntent> intent =
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
                             weak_ptr_factory_.GetWeakPtr(), intent->intent));
          return;
        }
      }
    }
#endif  // BUILDFLAG(IS_CHROMEOS)
  }
  OnDidCheckForIntentToPlayStore(/*intent=*/"",
                                 /*should_intent_to_store=*/false);
}

void FetchManifestAndInstallCommand::OnDidCheckForIntentToPlayStore(
    const std::string& intent,
    bool should_intent_to_store) {
  if (IsWebContentsDestroyed()) {
    Abort(webapps::InstallResultCode::kWebContentsDestroyed);
    return;
  }
#if BUILDFLAG(IS_CHROMEOS)
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
#endif  // BUILDFLAG(IS_CHROMEOS)

  data_retriever_->GetIcons(
      web_contents_.get(), icons_from_manifest_,
      skip_page_favicons_on_initial_download_,
      /*fail_all_if_any_fail=*/false,
      base::BindOnce(
          &FetchManifestAndInstallCommand::OnIconsRetrievedShowDialog,
          weak_ptr_factory_.GetWeakPtr()));
}

void FetchManifestAndInstallCommand::OnIconsRetrievedShowDialog(
    IconsDownloadedResult result,
    IconsMap icons_map,
    DownloadedIconsHttpResults icons_http_results) {
  if (IsWebContentsDestroyed()) {
    Abort(webapps::InstallResultCode::kWebContentsDestroyed);
    return;
  }
  base::Value::Dict* icons_downloaded =
      GetMutableDebugValue().EnsureDict("icons_retrieved");
  for (const auto& [url, bitmap_vector] : icons_map) {
    base::Value::List* sizes = icons_downloaded->EnsureList(url.spec());
    for (const SkBitmap& bitmap : bitmap_vector) {
      sizes->Append(bitmap.width());
    }
  }

  CHECK(web_app_info_);

  // In kUseFallbackInfoWhenNotInstallable mode, we skip favicons if the
  // manifest looks valid. However, if the icon download fails, we are no longer
  // installable & thus fall back to favicons.
  if (skip_page_favicons_on_initial_download_ &&
      valid_manifest_for_crafted_web_app_ && icons_map.empty() &&
      fallback_behavior_ ==
          FallbackBehavior::kUseFallbackInfoWhenNotInstallable) {
    GetMutableDebugValue().Set("used_fallback_after_icon_download_failed",
                               true);
    valid_manifest_for_crafted_web_app_ = false;
    web_app_info_->is_diy_app = true;
    GetMutableDebugValue().Set("is_diy_app", true);
    data_retriever_->GetIcons(
        web_contents_.get(), {},
        /*skip_page_favicons=*/false,
        /*fail_all_if_any_fail=*/false,
        base::BindOnce(
            &FetchManifestAndInstallCommand::OnIconsRetrievedShowDialog,
            weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  PopulateProductIcons(web_app_info_.get(), &icons_map);
  PopulateOtherIcons(web_app_info_.get(), icons_map);

  RecordDownloadedIconsResultAndHttpStatusCodes(result, icons_http_results);
  install_error_log_entry_.LogDownloadedIconsErrors(
      *web_app_info_, result, icons_map, icons_http_results);

  if (!dialog_callback_) {
    OnDialogCompleted(/*user_accepted=*/true, std::move(web_app_info_));
  } else {
    std::move(dialog_callback_)
        .Run((!screenshot_sizes_.empty() ? weak_ptr_factory_.GetWeakPtr()
                                         : nullptr),
             web_contents_.get(), std::move(web_app_info_),
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

  finalize_options.install_state =
      proto::InstallState::INSTALLED_WITH_OS_INTEGRATION;
  finalize_options.overwrite_existing_manifest_fields = true;
  finalize_options.add_to_applications_menu = true;
  finalize_options.add_to_desktop = true;
  finalize_options.add_to_quick_launch_bar = kAddAppsToQuickLaunchBarByDefault;

  DCHECK(app_lock_);
  app_lock_->install_finalizer().FinalizeInstall(
      *web_app_info_, finalize_options,
      base::BindOnce(
          &FetchManifestAndInstallCommand::OnInstallFinalizedMaybeReparentTab,
          weak_ptr_factory_.GetWeakPtr()));
}

void FetchManifestAndInstallCommand::OnInstallFinalizedMaybeReparentTab(
    const webapps::AppId& app_id,
    webapps::InstallResultCode code) {
  if (IsWebContentsDestroyed()) {
    Abort(webapps::InstallResultCode::kWebContentsDestroyed);
    return;
  }

  if (code != webapps::InstallResultCode::kSuccessNewInstall) {
    Abort(code);
    return;
  }

  // Stop observing the web contents to prevent cancellation when reparenting.
  Observe(nullptr);

  RecordWebAppInstallationTimestamp(
      Profile::FromBrowserContext(web_contents_.get()->GetBrowserContext())
          ->GetPrefs(),
      app_id, install_surface_);

  bool is_shortcut_created = IsShortcutCreated(app_lock_->registrar(), app_id);
  CHECK(app_lock_);

  const bool can_reparent_tab =
      app_lock_->ui_manager().CanReparentAppTabToWindow(
          app_id, is_shortcut_created, web_contents_.get());

  SCOPED_CRASH_KEY_NUMBER("PWA", "install_surface",
                          static_cast<int>(install_surface_));
  SCOPED_CRASH_KEY_STRING256(
      "PWA", "install_url",
      web_contents_.get()->GetLastCommittedURL().possibly_invalid_spec());
  SCOPED_CRASH_KEY_STRING64("PWA", "install_app_id", app_id);
  const WebAppTabHelper* tab_helper =
      WebAppTabHelper::FromWebContents(web_contents_.get());
  if (tab_helper) {
    SCOPED_CRASH_KEY_STRING64("PWA", "source_window_app_id",
                              tab_helper->window_app_id().value_or("<none>"));
  }

  if (can_reparent_tab &&
      (web_app_info_->user_display_mode != mojom::UserDisplayMode::kBrowser) &&
      (install_surface_ != webapps::WebappInstallSource::DEVTOOLS)) {
    app_lock_->ui_manager().ReparentAppTabToWindow(web_contents_.get(), app_id,
                                                   is_shortcut_created);
  }

  OnInstallCompleted(app_id, code);
}

void FetchManifestAndInstallCommand::OnInstallCompleted(
    const webapps::AppId& app_id,
    webapps::InstallResultCode code) {
  if (base::FeatureList::IsEnabled(features::kRecordWebAppDebugInfo)) {
    if (install_error_log_entry_.HasErrorDict()) {
      command_manager()->LogToInstallManager(
          install_error_log_entry_.TakeErrorDict());
    }
  }
  GetMutableDebugValue().Set("result_code", base::ToString(code));

  webapps::InstallableMetrics::TrackInstallResult(webapps::IsSuccess(code),
                                                  install_surface_);
  MeasureUserInstalledAppHistogram(code);
  CompleteAndSelfDestruct(webapps::IsSuccess(code) ? CommandResult::kSuccess
                                                   : CommandResult::kFailure,
                          app_id, code);
}

void FetchManifestAndInstallCommand::MeasureUserInstalledAppHistogram(
    webapps::InstallResultCode code) {
  if (!web_app_info_) {
    RecordInstallMetrics(InstallCommand::kFetchManifestAndInstall,
                         WebAppType::kUnknown, code, install_surface_);
    return;
  }
  RecordInstallMetrics(
      InstallCommand::kFetchManifestAndInstall,
      web_app_info_->is_diy_app ? WebAppType::kDiyApp : WebAppType::kCraftedApp,
      code, install_surface_);

  bool is_new_success_install = webapps::IsNewInstall(code);
  if (web_app_info_->is_diy_app) {
    base::UmaHistogramBoolean("WebApp.NewDiyAppInstalled.ByUser",
                              is_new_success_install);
  } else {
    base::UmaHistogramBoolean("WebApp.NewCraftedAppInstalled.ByUser",
                              is_new_success_install);
  }
}

void FetchManifestAndInstallCommand::StartPreloadingScreenshots() {
  CHECK(opt_manifest_);
  int count_screenshots = 0;
  for (const auto& screenshot : opt_manifest_->screenshots) {
    if (!IsValidScreenshotForDownload(screenshot)) {
      continue;
    }

    // Filter out too large screenshots earlier, so that the number of "spots"
    // to be used in the detailed install dialog is accurately identified.
    bool should_skip_large_screenshot = false;
    for (const gfx::Size& size : screenshot->image.sizes) {
      if (size.width() > webapps::kMaximumScreenshotSizeInPx ||
          size.height() > webapps::kMaximumScreenshotSizeInPx) {
        should_skip_large_screenshot = true;
        break;
      }
    }
    if (should_skip_large_screenshot) {
      continue;
    }

    if (++count_screenshots > webapps::kMaximumNumOfScreenshots) {
      break;
    }

    // Since narrow screenshots are filtered out, this is guaranteed to return
    // either the minimum screen shot size, or the width.
    int ideal_size = ComputeIdealScreenshotSize(screenshot);
    gfx::Size size_to_use = (ideal_size == webapps::kMinimumScreenshotSizeInPx)
                                ? gfx::Size(ideal_size, ideal_size)
                                : screenshot->image.sizes[0];
    screenshot_sizes_.push_back(size_to_use);

    // Suppress console warnings by the `ManifestIconDownloader` if installation
    // is triggered for a chrome:// url.
    bool suppress_warnings =
        screenshot->image.src.SchemeIs(content::kChromeUIScheme);

    // Do not pass in a maximum icon size so that screenshots larger than
    // kMaximumScreenshotSizeInPx are not downscaled to the maximum size by
    // `ManifestIconDownloader::Download`. Screenshots with size larger than
    // kMaximumScreenshotSizeInPx are already filtered out at this point.
    content::ManifestIconDownloader::Download(
        web_contents(), screenshot->image.src, ideal_size,
        webapps::kMinimumScreenshotSizeInPx,
        /*maximum_icon_size_in_px=*/0,
        base::BindOnce(&FetchManifestAndInstallCommand::OnScreenshotFetched,
                       weak_ptr_factory_.GetWeakPtr(), count_screenshots - 1,
                       screenshot->label),
        /*square_only=*/false, content::GlobalRenderFrameHostId(),
        suppress_warnings);
  }
}

void FetchManifestAndInstallCommand::OnScreenshotFetched(
    int index,
    std::optional<std::u16string> label,
    const SkBitmap& bitmap) {
  if (bitmap.drawsNothing()) {
    return;
  }

  screenshots_downloaded_[index] = std::tie(bitmap, label);
  auto pending_callback_it = pending_screenshot_callbacks_.find(index);
  if (pending_callback_it != pending_screenshot_callbacks_.end()) {
    std::move(pending_callback_it->second).Run(bitmap, label);
  }
}

}  // namespace web_app
