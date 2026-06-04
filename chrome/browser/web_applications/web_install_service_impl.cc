// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_install_service_impl.h"

#include <algorithm>
#include <optional>
#include <vector>

#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/commands/web_install_from_url_command.h"
#include "chrome/browser/web_applications/icons/icon_masker.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/model/app_installed_by.h"
#include "chrome/browser/web_applications/model/dialog_image_info.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"
#include "chrome/browser/web_applications/web_contents/web_contents_manager.h"
#include "components/permissions/permission_request.h"
#include "components/ukm/app_source_url_recorder.h"
#include "components/webapps/browser/banners/app_banner_manager.h"
#include "components/webapps/browser/banners/installable_web_app_check_result.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/installable/installable_params.h"
#include "components/webapps/browser/installable/ml_install_operation_tracker.h"
#include "components/webapps/browser/installable/ml_installability_promoter.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_descriptor_util.h"
#include "content/public/browser/permission_request_description.h"
#include "content/public/browser/permission_result.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "third_party/blink/public/mojom/manifest/manifest_manager.mojom.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"
#include "third_party/blink/public/mojom/web_install/web_install.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace web_app {

namespace {

using PermissionStatus = blink::mojom::PermissionStatus;

constexpr SquareSizePx kIconSizeForLaunchDialog = 32;
constexpr char kInstallApiResultUma[] = "WebApp.WebInstallApi.Result";
constexpr char kInstallApiTypeUma[] = "WebApp.WebInstallApi.InstallType";
constexpr char kInstallElementResultUma[] = "WebApp.WebInstallElement.Result";
constexpr char kInstallElementTypeUma[] =
    "WebApp.WebInstallElement.InstallType";

// Rate limiting defaults for cross-origin IsInstalled queries.
size_t g_max_cross_origin_queries = 100;
base::TimeDelta g_min_cross_origin_query_interval = base::Seconds(1);

// Checks if an app is installed based on `manifest_id`, if possible. Otherwise
// falls back to `install_target`. Used by the background doc install path.
// These are allowed to use unsafe registrar accesses, as this is the first step
// in a launch flow, and we later queue a command to launch, which will safely
// recheck the app's state in the registrar, and fail gracefully if it's no
// longer installed.
std::optional<webapps::AppId> IsAppInstalled(
    WebAppProvider& provider,
    const GURL& install_target,
    const std::optional<GURL>& manifest_id) {
  // Only consider apps that launch in a standalone window, or were installed
  // by the user.
  WebAppFilter filter = WebAppFilter::LaunchableFromInstallApi();

  // If the developer provided a manifest ID, use it to look up the app. This
  // avoids issues with nested app scopes and `install_target` potentially
  // launching the wrong app.
  if (manifest_id) {
    std::optional<webapps::ManifestId> valid_manifest_id =
        webapps::ManifestId::Create(manifest_id.value());
    if (!valid_manifest_id.has_value()) {
      return std::nullopt;
    }

    webapps::AppId app_id_from_manifest_id =
        GenerateAppIdFromManifestId(valid_manifest_id.value());

    bool found_app =
        provider.registrar_unsafe().AppMatches(app_id_from_manifest_id, filter);

    return found_app ? std::optional<webapps::AppId>(app_id_from_manifest_id)
                     : std::nullopt;
  }

  // No `manifest_id` was provided. Check for the app by `install_target`. This
  // is less accurate and may result in another app being launched.
  return provider.registrar_unsafe().FindBestAppWithUrlInScope(install_target,
                                                               filter);
}

void CheckInstalledByAndMaybeUpdate(const base::Time& api_call_time,
                                    const GURL& requesting_page,
                                    const webapps::AppId& app_id,
                                    AppLock& lock,
                                    base::DictValue& debug_value) {
  ScopedRegistryUpdate update = lock.sync_bridge().BeginUpdate();
  WebApp* app_to_update = update->UpdateApp(app_id);
  if (!app_to_update) {
    // App was uninstalled before we could update it.
    return;
  }
  app_to_update->AddInstalledByInfo(
      web_app::AppInstalledBy(api_call_time, requesting_page));
}

void EmitInstallResultUma(bool triggered_from_element,
                          web_app::WebInstallServiceResult result) {
  base::UmaHistogramEnumeration(
      triggered_from_element ? kInstallElementResultUma : kInstallApiResultUma,
      result);
  base::UmaHistogramEnumeration(
      base::StrCat({"WebApp.WebInstallService.",
                    triggered_from_element ? "Element" : "Api", ".Result"}),
      result);
}

void EmitInstallTypeUma(bool triggered_from_element,
                        web_app::WebInstallServiceType install_type) {
  base::UmaHistogramEnumeration(
      triggered_from_element ? kInstallElementTypeUma : kInstallApiTypeUma,
      install_type);
  base::UmaHistogramEnumeration(
      base::StrCat({"WebApp.WebInstallService.",
                    triggered_from_element ? "Element" : "Api",
                    ".InstallType"}),
      install_type);
}

}  // namespace

WebInstallServiceImpl::WebInstallServiceImpl(
    content::RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<blink::mojom::WebInstallService> receiver)
    : content::DocumentService<blink::mojom::WebInstallService>(
          render_frame_host,
          std::move(receiver)),
      frame_routing_id_(render_frame_host.GetGlobalId()),
      last_committed_url_(render_frame_host.GetLastCommittedURL()) {}

WebInstallServiceImpl::~WebInstallServiceImpl() = default;

// static
void WebInstallServiceImpl::CreateIfAllowed(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::WebInstallService> receiver) {
  CHECK(render_frame_host);

  CHECK(base::FeatureList::IsEnabled(blink::features::kWebAppInstallation) ||
        base::FeatureList::IsEnabled(blink::features::kInstallElement));

  // This class is created only on the primary main frame.
  if (!render_frame_host->IsInPrimaryMainFrame()) {
    receiver.reset();
    return;
  }

  if (!render_frame_host->GetLastCommittedURL().SchemeIsHTTPOrHTTPS()) {
    receiver.reset();
    return;
  }

  new WebInstallServiceImpl(*render_frame_host, std::move(receiver));
}

// static
base::AutoReset<size_t>
WebInstallServiceImpl::SetMaxCrossOriginQueriesForTesting(  // IN-TEST
    size_t max_queries) {
  return base::AutoReset<size_t>(&g_max_cross_origin_queries, max_queries);
}

// static
base::AutoReset<base::TimeDelta>
WebInstallServiceImpl::SetMinCrossOriginQueryIntervalForTesting(  // IN-TEST
    base::TimeDelta interval) {
  return base::AutoReset<base::TimeDelta>(&g_min_cross_origin_query_interval,
                                          interval);
}

void WebInstallServiceImpl::IsInstalled(blink::mojom::InstallOptionsPtr options,
                                        IsInstalledCallback callback) {
  GURL install_target;
  std::optional<GURL> manifest_id;
  if (options) {
    install_target = GURL(options->install_url);
    manifest_id = options->manifest_id;
  } else {
    install_target = last_committed_url_;
  }

  // Exclude invalid URLs, file://, chrome://, etc.
  if (!install_target.is_valid() || !install_target.SchemeIsHTTPOrHTTPS()) {
    std::move(callback).Run(false);
    return;
  }
  if (manifest_id.has_value() &&
      (!manifest_id->is_valid() || !manifest_id->SchemeIsHTTPOrHTTPS())) {
    std::move(callback).Run(false);
    return;
  }

  // `IsAppInstalled` queries by `manifest_id` if available, otherwise
  // `install_target`.
  const GURL lookup_url = manifest_id.value_or(install_target);
  const url::Origin document_origin =
      render_frame_host().GetLastCommittedOrigin();

  // Same-origin queries are not rate limited.
  if (document_origin.IsSameOriginWith(url::Origin::Create(lookup_url))) {
    RunIsInstalledLookup(std::move(install_target), std::move(manifest_id),
                         std::move(callback));
    return;
  }

  // Rate limit queries that could expose cross-origin app installation state.

  // Per-document cap. Counts attempts (not accepts) so that abuse permanently
  // exhausts the budget. Queries are counted when received, not when they're
  // dispatched.
  cross_origin_query_count_++;
  if (cross_origin_query_count_ > g_max_cross_origin_queries) {
    std::move(callback).Run(false);
    return;
  }

  // Compute how long to defer this query. Each cross-origin query is
  // dispatched at its own reserved slot, with slots spaced by
  // `g_min_cross_origin_query_interval`.
  // `next_cross_origin_query_dispatch_time_` tracks the next available slot.
  // This query takes that slot, then advances it for the next query.
  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeDelta delay =
      std::max(base::TimeDelta(), next_cross_origin_query_dispatch_time_ - now);

  // Calculate the next slot. `max(now, ...)` re-bases at `now` to account for
  // queries received after an extended idle period.
  next_cross_origin_query_dispatch_time_ =
      std::max(now, next_cross_origin_query_dispatch_time_) +
      g_min_cross_origin_query_interval;

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&WebInstallServiceImpl::RunIsInstalledLookup,
                     weak_ptr_factory_.GetWeakPtr(), std::move(install_target),
                     std::move(manifest_id), std::move(callback)),
      delay);
}

void WebInstallServiceImpl::RunIsInstalledLookup(
    GURL install_target,
    std::optional<GURL> manifest_id,
    IsInstalledCallback callback) {
  auto* provider = WebAppProvider::GetForWebApps(
      Profile::FromBrowserContext(render_frame_host().GetBrowserContext()));
  // `kWebAppInstallation` or `kInstallElement` is guaranteed to be enabled at
  // this point, however the provider may be null (e.g. Incognito).
  if (!provider) {
    std::move(callback).Run(false);
    return;
  }
  std::optional<webapps::AppId> app_id =
      IsAppInstalled(*provider, install_target, manifest_id);

  std::move(callback).Run(app_id.has_value());
}

void WebInstallServiceImpl::Install(blink::mojom::InstallOptionsPtr options,
                                    InstallCallback callback) {
  InstallInternal(std::move(options), std::move(callback),
                  /*triggered_from_element=*/false);
}

void WebInstallServiceImpl::InstallFromElement(
    blink::mojom::InstallOptionsPtr options,
    InstallCallback callback) {
  InstallInternal(std::move(options), std::move(callback),
                  /*triggered_from_element=*/true);
}

void WebInstallServiceImpl::InstallInternal(
    blink::mojom::InstallOptionsPtr options,
    InstallCallback callback,
    bool triggered_from_element) {
  web_app::WebInstallServiceType install_type =
      options ? web_app::WebInstallServiceType::kBackgroundDocument
              : web_app::WebInstallServiceType::kCurrentDocument;
  EmitInstallTypeUma(triggered_from_element, install_type);

  if (IsInstallInProgress()) {
    EmitInstallResultUma(triggered_from_element,
                         web_app::WebInstallServiceResult::kInstallInProgress);
    std::move(callback).Run(blink::mojom::WebInstallServiceResult::kAbortError,
                            GURL());
    return;
  }
  base::ScopedClosureRunner install_guard = ReserveInstallInProgress();

  // Create source ids for UKM logging.
  ukm::SourceId requesting_page_source_id =
      options ? render_frame_host().GetPageUkmSourceId()
              : ukm::kInvalidSourceId;
  ukm::SourceId installed_app_source_id =
      options ? ukm::AppSourceUrlRecorder::GetSourceIdForPWA(
                    GURL(options->install_url))
              : ukm::kInvalidSourceId;

  // Wrap the blink callback in another that accepts all the information needed
  // to log our UMAs and UKMs, reset `install_guard`, then run the blink
  // callback.
  auto callback_with_metrics = base::BindOnce(
      [](InstallCallback callback, base::ScopedClosureRunner install_guard,
         bool triggered_from_element, ukm::SourceId requesting_page_source_id,
         ukm::SourceId installed_app_source_id,
         web_app::WebInstallServiceResult metrics_result,
         blink::mojom::WebInstallServiceResult install_result,
         std::optional<webapps::ManifestId> manifest_id_result) {
        // TODO(crbug.com/477993292): Reevaluate/clean up web install telemetry
        // after Origin Trials.
        EmitInstallResultUma(triggered_from_element, metrics_result);

        // Record UKMs for background document installs.
        if (requesting_page_source_id != ukm::kInvalidSourceId &&
            installed_app_source_id != ukm::kInvalidSourceId) {
          auto requesting_page_builder =
              ukm::builders::WebApp_WebInstall(requesting_page_source_id);
          if (triggered_from_element) {
            requesting_page_builder.SetElementResultByRequestingPage(
                static_cast<int>(metrics_result));
          } else {
            requesting_page_builder.SetResultByRequestingPage(
                static_cast<int>(metrics_result));
          }
          requesting_page_builder.Record(ukm::UkmRecorder::Get());

          // The UKM for the installed app must log source type APP_ID.
          CHECK(ukm::GetSourceIdType(installed_app_source_id) ==
                ukm::SourceIdType::APP_ID);
          auto installed_app_builder =
              ukm::builders::WebApp_WebInstall(installed_app_source_id);
          if (triggered_from_element) {
            installed_app_builder.SetElementResultByInstalledApp(
                static_cast<int>(metrics_result));
          } else {
            installed_app_builder.SetResultByInstalledApp(
                static_cast<int>(metrics_result));
          }
          installed_app_builder.Record(ukm::UkmRecorder::Get());
        }
        std::move(callback).Run(install_result,
                                manifest_id_result.has_value()
                                    ? manifest_id_result->value()
                                    : GURL());
      },
      std::move(callback), std::move(install_guard), triggered_from_element,
      requesting_page_source_id, installed_app_source_id);

  auto* rfh = content::RenderFrameHost::FromID(frame_routing_id_);
  if (!rfh) {
    std::move(callback_with_metrics)
        .Run(web_app::WebInstallServiceResult::kUnexpectedFailure,
             blink::mojom::WebInstallServiceResult::kAbortError, std::nullopt);
    return;
  }

  GURL install_target =
      options ? GURL(options->install_url) : last_committed_url_;

  // Do not allow installation of file:// or chrome:// urls.
  if (!install_target.SchemeIsHTTPOrHTTPS()) {
    std::move(callback_with_metrics)
        .Run(web_app::WebInstallServiceResult::kUnexpectedFailure,
             blink::mojom::WebInstallServiceResult::kAbortError, std::nullopt);
    return;
  }

  // Installing web apps is not supported from off-the-record profiles. Show
  // the install not supported dialog.
  auto* profile = Profile::FromBrowserContext(rfh->GetBrowserContext());
  if (!AreWebAppsUserInstallable(profile)) {
    WebAppUiManager::TriggerInstallNotSupportedDialog(
        content::WebContents::FromRenderFrameHost(rfh), profile,
        base::BindOnce(
            &WebInstallServiceImpl::OnInstallNotSupportedDialogClosed,
            weak_ptr_factory_.GetWeakPtr(), std::move(callback_with_metrics)));
    return;
  }

  // Initiate installation of the current document if no install options were
  // given.
  if (!options) {
    TryInstallCurrentDocument(std::move(callback_with_metrics));

    // Current document installs don't require the permissions checking code.
    return;
  }

  // Skip requesting permission in two cases:
  // 1. The install URL matches the current document URL (user is installing
  //    the page they're currently on, just using background install syntax).
  // 2. Install triggered from the <install> element.
  // In both cases, the install dialog is always shown.
  if (triggered_from_element || install_target == last_committed_url_) {
    OnPermissionDecided(
        std::move(options), std::move(callback_with_metrics),
        std::vector<content::PermissionResult>({content::PermissionResult(
            PermissionStatus::GRANTED,
            content::PermissionStatusSource::UNSPECIFIED)}));
    return;
  }

  // Verify that the calling app has the Web Install permissions policy set.
  if (!rfh->IsFeatureEnabled(
          network::mojom::PermissionsPolicyFeature::kWebAppInstallation)) {
    std::move(callback_with_metrics)
        .Run(web_app::WebInstallServiceResult::kPermissionDenied,
             blink::mojom::WebInstallServiceResult::kAbortError, std::nullopt);
    return;
  }

  RequestWebInstallPermission(
      base::BindOnce(&WebInstallServiceImpl::OnPermissionDecided,
                     weak_ptr_factory_.GetWeakPtr(), std::move(options),
                     std::move(callback_with_metrics)));
}

bool WebInstallServiceImpl::IsInstallInProgress() const {
  return install_in_progress_;
}

base::ScopedClosureRunner WebInstallServiceImpl::ReserveInstallInProgress() {
  install_in_progress_ = true;
  return base::ScopedClosureRunner(
      base::BindOnce(&WebInstallServiceImpl::ReleaseInstallInProgress,
                     weak_ptr_factory_.GetWeakPtr()));
}

void WebInstallServiceImpl::ReleaseInstallInProgress() {
  install_in_progress_ = false;
}

void WebInstallServiceImpl::OnInstallNotSupportedDialogClosed(
    InstallCallbackWithMetrics callback_with_metrics) {
  std::move(callback_with_metrics)
      .Run(web_app::WebInstallServiceResult::kUnsupportedProfile,
           blink::mojom::WebInstallServiceResult::kAbortError, std::nullopt);
}

void WebInstallServiceImpl::TryInstallCurrentDocument(
    InstallCallbackWithMetrics callback_with_metrics) {
  auto* web_contents =
      content::WebContents::FromRenderFrameHost(&render_frame_host());
  auto* provider = WebAppProvider::GetForWebContents(web_contents);
  CHECK(provider);

  // Check if the current document is already installed.
  const webapps::AppId* app_id =
      web_app::WebAppTabHelper::GetAppId(web_contents);
  if (!app_id) {
    // The current document is not installed yet. Retrieve the manifest to
    // perform id validation checks.

    // Create a new data retriever for this call. The retriever is stored in
    // `data_retrievers_` so it gets destroyed when this service is destroyed,
    // or when the callback completes.
    std::unique_ptr<WebAppDataRetriever> data_retriever =
        provider->web_contents_manager().CreateDataRetriever();
    base::WeakPtr<WebAppDataRetriever> weak_data_retriever =
        data_retriever->GetWeakPtr();
    data_retrievers_.insert(std::move(data_retriever));
    weak_data_retriever->GetPrimaryPageFirstSpecifiedManifest(
        *web_contents,
        base::BindOnce(
            &WebInstallServiceImpl::OnGotManifestForCurrentDocumentInstall,
            weak_ptr_factory_.GetWeakPtr(), std::move(callback_with_metrics),
            provider, weak_data_retriever));
    return;
  }
  // If the current document that is trying to install is currently in a PWA
  // window, return kSuccessAlreadyInstalled.
  web_app::WebAppTabHelper* tab_helper =
      web_app::WebAppTabHelper::FromWebContents(web_contents);
  if (tab_helper->is_in_app_window()) {
    OnAppInstalled(std::move(callback_with_metrics), *app_id,
                   webapps::InstallResultCode::kSuccessAlreadyInstalled);
    return;
  }

  provider->scheduler().ScheduleCallback<AppLock>(
      "WebInstallServiceImpl::TryInstallCurrentDocument",
      AppLockDescription(*app_id),
      base::BindOnce(&WebInstallServiceImpl::CheckForInstalledAppMaybeLaunch,
                     weak_ptr_factory_.GetWeakPtr(), web_contents,
                     std::move(callback_with_metrics)),
      /*on_complete=*/base::DoNothing());
}

void WebInstallServiceImpl::CheckForInstalledAppMaybeLaunch(
    content::WebContents* web_contents,
    InstallCallbackWithMetrics callback_with_metrics,
    AppLock& lock,
    base::DictValue& debug_value) {
  // Now that we've locked the app, re-confirm the current document is still
  // already installed on the current device.
  const webapps::AppId* app_id =
      web_app::WebAppTabHelper::GetAppId(web_contents);
  if (!app_id || !lock.registrar().AppMatches(
                     *app_id, web_app::WebAppFilter::InstalledInChrome())) {
    std::move(callback_with_metrics)
        .Run(web_app::WebInstallServiceResult::kUnexpectedFailure,
             blink::mojom::WebInstallServiceResult::kAbortError, std::nullopt);
    return;
  }

  auto* provider = WebAppProvider::GetForWebContents(web_contents);
  CHECK(provider);

  // Now that we know the app is already installed, show the intent picker.
  provider->ui_manager().ShowIntentPicker(
      web_contents->GetURL(), web_contents,
      base::BindOnce(&WebInstallServiceImpl::OnIntentPickerMaybeLaunched,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback_with_metrics), *app_id));
}

void WebInstallServiceImpl::OnIntentPickerMaybeLaunched(
    InstallCallbackWithMetrics callback_with_metrics,
    webapps::AppId app_id,
    bool user_chose_to_open) {
  // If the user chose to open the app in the intent picker, return success.
  // Otherwise, return an abort error.
  if (user_chose_to_open) {
    OnAppInstalled(std::move(callback_with_metrics), app_id,
                   webapps::InstallResultCode::kSuccessAlreadyInstalled);
  } else {
    std::move(callback_with_metrics)
        .Run(web_app::WebInstallServiceResult::kSuccessAlreadyInstalled,
             blink::mojom::WebInstallServiceResult::kAbortError, std::nullopt);
  }
}

void WebInstallServiceImpl::OnGotManifestForCurrentDocumentInstall(
    InstallCallbackWithMetrics callback_with_metrics,
    WebAppProvider* provider,
    base::WeakPtr<WebAppDataRetriever> data_retriever,
    const base::expected<blink::mojom::ManifestPtr,
                         blink::mojom::RequestManifestErrorPtr>& result) {
  // Remove the data retriever from the set now that the callback has completed.
  if (data_retriever) {
    data_retrievers_.erase(data_retriever.get());
  }

  // Report a data error if no manifest was returned.
  if (!result.has_value() || blink::IsEmptyManifest(result.value())) {
    std::move(callback_with_metrics)
        .Run(web_app::WebInstallServiceResult::kInstallCommandFailed,
             blink::mojom::WebInstallServiceResult::kDataError, std::nullopt);
    return;
  }

  const blink::mojom::ManifestPtr& manifest = result.value();

  // Ensure that the manifest is from the same trusted origin as the current
  // document.
  if (!origin().IsSameOriginWith(manifest->id)) {
    std::move(callback_with_metrics)
        .Run(web_app::WebInstallServiceResult::kInstallCommandFailed,
             blink::mojom::WebInstallServiceResult::kDataError, std::nullopt);
    return;
  }

  // The manifest must have a developer-specified id since the current document
  // version of navigator.install does not take a `manifest_id`.
  if (!manifest->has_custom_id) {
    std::move(callback_with_metrics)
        .Run(web_app::WebInstallServiceResult::kNoCustomManifestId,
             blink::mojom::WebInstallServiceResult::kDataError, std::nullopt);
    return;
  }

  // Check if the current web contents already has an install in progress.
  // This protects against spam-calling navigator.install() and triggering
  // multiple install dialogs.
  // TODO(crbug.com/478893336): Remove these checks once
  // CreateWebAppFromManifest is updated to always run its callback.
  auto* web_contents =
      content::WebContents::FromRenderFrameHost(&render_frame_host());
  webapps::MLInstallabilityPromoter* promoter =
      webapps::MLInstallabilityPromoter::FromWebContents(web_contents);
  if (promoter && promoter->HasCurrentInstall()) {
    std::move(callback_with_metrics)
        .Run(web_app::WebInstallServiceResult::kUnexpectedFailure,
             blink::mojom::WebInstallServiceResult::kAbortError, std::nullopt);
    return;
  }

  if (provider->command_manager().IsInstallingForWebContents(web_contents)) {
    std::move(callback_with_metrics)
        .Run(web_app::WebInstallServiceResult::kUnexpectedFailure,
             blink::mojom::WebInstallServiceResult::kAbortError, std::nullopt);
    return;
  }

  provider->ui_manager().TriggerInstallDialog(
      content::WebContents::FromRenderFrameHost(&render_frame_host()),
      webapps::WebappInstallSource::WEB_INSTALL,
      base::BindOnce(&WebInstallServiceImpl::OnAppInstalled,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback_with_metrics)));
}

void WebInstallServiceImpl::RequestWebInstallPermission(
    base::OnceCallback<void(const std::vector<content::PermissionResult>&)>
        callback) {
  content::BrowserContext* const browser_context =
      render_frame_host().GetBrowserContext();
  if (!browser_context) {
    std::move(callback).Run(
        std::vector<content::PermissionResult>({content::PermissionResult(
            PermissionStatus::DENIED,
            content::PermissionStatusSource::UNSPECIFIED)}));
    return;
  }

  content::PermissionController* const permission_controller =
      browser_context->GetPermissionController();
  if (!permission_controller) {
    std::move(callback).Run(
        std::vector<content::PermissionResult>({content::PermissionResult(
            PermissionStatus::DENIED,
            content::PermissionStatusSource::UNSPECIFIED)}));
    return;
  }

  // Check if the permission status is already set.
  content::PermissionResult permission_status =
      permission_controller->GetPermissionResultForCurrentDocument(
          content::PermissionDescriptorUtil::
              CreatePermissionDescriptorForPermissionType(
                  blink::PermissionType::WEB_APP_INSTALLATION),
          &render_frame_host());
  switch (permission_status.status) {
    case PermissionStatus::GRANTED:
      std::move(callback).Run(
          std::vector<content::PermissionResult>({content::PermissionResult(
              PermissionStatus::GRANTED,
              content::PermissionStatusSource::UNSPECIFIED)}));
      return;
    case PermissionStatus::DENIED:
      std::move(callback).Run(
          std::vector<content::PermissionResult>({content::PermissionResult(
              PermissionStatus::DENIED,
              content::PermissionStatusSource::UNSPECIFIED)}));
      return;
    case PermissionStatus::ASK:
      break;
  }

  GURL requesting_origin = origin().GetURL();
  // User gesture requirement is enforced in NavigatorWebInstall::InstallImpl.
  permission_controller->RequestPermissionsFromCurrentDocument(
      &render_frame_host(),
      content::PermissionRequestDescription(
          content::PermissionDescriptorUtil::
              CreatePermissionDescriptorForPermissionType(
                  blink::PermissionType::WEB_APP_INSTALLATION),
          /*user_gesture=*/true, requesting_origin),
      std::move(callback));
}

void WebInstallServiceImpl::OnPermissionDecided(
    blink::mojom::InstallOptionsPtr install_options,
    InstallCallbackWithMetrics callback_with_metrics,
    const std::vector<content::PermissionResult>& permission_result) {
  CHECK(install_options);
  CHECK_EQ(permission_result.size(), 1u);

  if (permission_result[0].status != PermissionStatus::GRANTED) {
    std::move(callback_with_metrics)
        .Run(web_app::WebInstallServiceResult::kPermissionDenied,
             blink::mojom::WebInstallServiceResult::kAbortError, std::nullopt);
    return;
  }

  // Now that we have permission, verify that the current web contents is not
  // already involved in an install operation. This protects against showing
  // multiple dialogs, either install for the current or a background document,
  // or a background document launch.
  auto* web_contents =
      content::WebContents::FromRenderFrameHost(&render_frame_host());
  webapps::MLInstallabilityPromoter* promoter =
      webapps::MLInstallabilityPromoter::FromWebContents(web_contents);
  CHECK(promoter);
  if (promoter->HasCurrentInstall()) {
    // The current web contents is being installed via another method. Cancel
    // this background install/launch flow.
    std::move(callback_with_metrics)
        .Run(web_app::WebInstallServiceResult::kUnexpectedFailure,
             blink::mojom::WebInstallServiceResult::kAbortError, std::nullopt);
    return;
  }

  auto* profile =
      Profile::FromBrowserContext(render_frame_host().GetBrowserContext());
  auto* provider = WebAppProvider::GetForWebApps(profile);
  CHECK(provider);
  if (provider->command_manager().IsInstallingForWebContents(web_contents)) {
    // Another install is already scheduled on the current web contents.
    // Cancel this background install/launch flow.
    std::move(callback_with_metrics)
        .Run(web_app::WebInstallServiceResult::kUnexpectedFailure,
             blink::mojom::WebInstallServiceResult::kAbortError, std::nullopt);
    return;
  }

  // Check if the background document is already installed so we can show the
  // launch dialog instead of the install dialog. See definition for details
  // on how we check if the app is installed.
  std::optional<webapps::AppId> app_id = IsAppInstalled(
      *provider, install_options->install_url, install_options->manifest_id);
  if (app_id) {
    // See `IsAppInstalled` for why this can be unsafe.
    const GURL& installed_manifest_id =
        provider->registrar_unsafe().GetComputedManifestId(*app_id);
    CHECK(!installed_manifest_id.is_empty());
    std::optional<webapps::ManifestId> valid_manifest_id =
        webapps::ManifestId::Create(installed_manifest_id);
    CHECK(valid_manifest_id.has_value());
    // Get the information to display in the launch dialog.
    provider->scheduler().FetchInstallInfoFromInstallUrl(
        *valid_manifest_id, install_options->install_url,
        base::BindOnce(
            &WebInstallServiceImpl::OnInstallInfoFromInstallUrlFetched,
            weak_ptr_factory_.GetWeakPtr(), std::move(callback_with_metrics),
            *app_id, installed_manifest_id));
    return;
  }

  // `install_url` was not installed. Proceed with the background install.

  // Register the background install on the current web contents.
  std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker =
      promoter->RegisterCurrentInstallForWebContents(
          webapps::WebappInstallSource::WEB_INSTALL);

  provider->ui_manager().TriggerInstallDialogForBackgroundInstall(
      web_contents, std::move(install_tracker), install_options->install_url,
      install_options->manifest_id, last_committed_url_,
      base::BindOnce(&WebInstallServiceImpl::OnAppInstalled,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback_with_metrics)));
}

void WebInstallServiceImpl::OnInstallInfoFromInstallUrlFetched(
    InstallCallbackWithMetrics callback_with_metrics,
    webapps::AppId app_id,
    const GURL& manifest_id,
    std::unique_ptr<WebAppInstallInfo> install_info) {
  if (!install_info) {
    // Failed to fetch install info for the already installed app. For example,
    // redirecting URLs are not supported here so we can't get the install info.
    // TODO(crbug.com/471021583): Evaluate supporting redirects.
    std::move(callback_with_metrics)
        .Run(web_app::WebInstallServiceResult::kUnexpectedFailure,
             blink::mojom::WebInstallServiceResult::kDataError, std::nullopt);
    return;
  }
  // Choose the icon bitmap based on OS specific icon guidelines. See
  // crbug.com/423906188 for more information. Regardless of OS, we expect an
  // icon of size 32x32 to be available.
  DialogImageInfo dialog_info = install_info->GetIconBitmapsForSecureSurfaces();
  auto icon_it = dialog_info.bitmaps.find(kIconSizeForLaunchDialog);
  CHECK(icon_it != dialog_info.bitmaps.end());
  SkBitmap icon_bitmap_to_use = icon_it->second;

  // Name to display in the dialog.
  std::u16string app_title = install_info->title.value();
  base::TrimWhitespace(app_title, base::TRIM_ALL, &app_title);
  if (!dialog_info.is_maskable) {
    OnIconFinalizedTriggerDialog(std::move(callback_with_metrics), app_id,
                                 manifest_id, app_title,
                                 std::move(icon_bitmap_to_use));
    return;
  }

  web_app::MaskIconOnOs(
      std::move(icon_bitmap_to_use),
      base::BindOnce(&WebInstallServiceImpl::OnIconFinalizedTriggerDialog,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback_with_metrics), app_id, manifest_id,
                     app_title));
}

void WebInstallServiceImpl::OnIconFinalizedTriggerDialog(
    InstallCallbackWithMetrics callback_with_metrics,
    webapps::AppId app_id,
    const GURL& manifest_id,
    std::u16string app_title,
    const SkBitmap icon_to_use) {
  auto* profile =
      Profile::FromBrowserContext(render_frame_host().GetBrowserContext());
  auto* provider = WebAppProvider::GetForWebApps(profile);
  CHECK(provider);
  auto* web_contents =
      content::WebContents::FromRenderFrameHost(&render_frame_host());

  provider->ui_manager().TriggerLaunchDialogForBackgroundInstall(
      web_contents, app_id, profile, base::UTF16ToUTF8(app_title), icon_to_use,
      base::BindOnce(&WebInstallServiceImpl::OnBackgroundAppLaunchDialogClosed,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback_with_metrics), manifest_id));
}

void WebInstallServiceImpl::OnBackgroundAppLaunchDialogClosed(
    InstallCallbackWithMetrics callback_with_metrics,
    const GURL& manifest_id,
    bool accepted) {
  std::optional<webapps::ManifestId> valid_manifest_id =
      webapps::ManifestId::Create(manifest_id);

  // Update the installed_by field if the user accepted the launch.
  if (accepted && valid_manifest_id.has_value()) {
    auto* profile =
        Profile::FromBrowserContext(render_frame_host().GetBrowserContext());
    auto* provider = WebAppProvider::GetForWebApps(profile);
    CHECK(provider);

    webapps::AppId app_id =
        GenerateAppIdFromManifestId(valid_manifest_id.value());
    provider->scheduler().ScheduleCallback<AppLock>(
        "CheckInstalledByAndMaybeUpdate", AppLockDescription(app_id),
        base::BindOnce(&CheckInstalledByAndMaybeUpdate, provider->clock().Now(),
                       last_committed_url_, app_id),
        /*on_complete=*/base::DoNothing());
  }

  // For privacy reasons, only resolve with WebInstallServiceResult::kSuccess if
  // the user accepted.
  std::move(callback_with_metrics)
      .Run(web_app::WebInstallServiceResult::kSuccessAlreadyInstalled,
           (accepted && valid_manifest_id.has_value())
               ? blink::mojom::WebInstallServiceResult::kSuccess
               : blink::mojom::WebInstallServiceResult::kAbortError,
           (accepted && valid_manifest_id.has_value())
               ? std::optional<webapps::ManifestId>(valid_manifest_id.value())
               : std::nullopt);
}

void WebInstallServiceImpl::OnAppInstalled(
    InstallCallbackWithMetrics callback_with_metrics,
    const webapps::AppId& app_id,
    webapps::InstallResultCode code) {
  blink::mojom::WebInstallServiceResult install_result;
  web_app::WebInstallServiceResult uma_result;
  std::optional<webapps::ManifestId> manifest_id_result;

  if (webapps::IsSuccess(code)) {
    install_result = blink::mojom::WebInstallServiceResult::kSuccess;

    if (webapps::IsNewInstall(code)) {
      uma_result = web_app::WebInstallServiceResult::kSuccess;
    } else if (code == webapps::InstallResultCode::kSuccessAlreadyInstalled) {
      uma_result = web_app::WebInstallServiceResult::kSuccessAlreadyInstalled;
    }

    auto* profile =
        Profile::FromBrowserContext(render_frame_host().GetBrowserContext());
    auto* provider = WebAppProvider::GetForWebApps(profile);
    CHECK(provider);
    manifest_id_result =
        webapps::ManifestId::Create(
            provider->registrar_unsafe().GetComputedManifestId(app_id));
    CHECK(manifest_id_result.has_value());

  } else {
    switch (code) {
      case webapps::InstallResultCode::kNoCustomManifestId:
        install_result = blink::mojom::WebInstallServiceResult::kDataError;
        uma_result = web_app::WebInstallServiceResult::kNoCustomManifestId;
        break;
      case webapps::InstallResultCode::kManifestIdMismatch:
        install_result = blink::mojom::WebInstallServiceResult::kDataError;
        uma_result = web_app::WebInstallServiceResult::kManifestIdMismatch;
        break;
      case webapps::InstallResultCode::kUserInstallDeclined:
        install_result = blink::mojom::WebInstallServiceResult::kAbortError;
        uma_result = web_app::WebInstallServiceResult::kCanceledByUser;
        break;
      // Signaling developer action to fix issues with provided data.
      case webapps::InstallResultCode::kInstallURLLoadFailed:
      case webapps::InstallResultCode::kInstallURLRedirected:
      case webapps::InstallResultCode::kNotValidManifestForWebApp:
      case webapps::InstallResultCode::kNotInstallable:
        install_result = blink::mojom::WebInstallServiceResult::kDataError;
        uma_result = web_app::WebInstallServiceResult::kInstallCommandFailed;
        break;
      default:
        install_result = blink::mojom::WebInstallServiceResult::kAbortError;
        // Catch all for any other failures during the install commands.
        // For more fine grained error codes, see the histogram emitted by
        // the commands - See
        // "WebApp.InstallCommand{InstallCommand}{WebAppType}.ResultCode",
        // and `RecordInstallMetrics` in `command_metrics.h`.
        uma_result = web_app::WebInstallServiceResult::kInstallCommandFailed;
        break;
    }
  }

  std::move(callback_with_metrics)
      .Run(uma_result, install_result, manifest_id_result);
}

}  // namespace web_app
