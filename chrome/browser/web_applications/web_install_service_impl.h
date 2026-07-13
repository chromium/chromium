// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_INSTALL_SERVICE_IMPL_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_INSTALL_SERVICE_IMPL_H_

#include <optional>
#include <vector>

#include "base/auto_reset.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/document_service.h"
#include "content/public/browser/permission_result.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"
#include "third_party/blink/public/mojom/manifest/manifest_manager.mojom-forward.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"
#include "third_party/blink/public/mojom/web_install/web_install.mojom.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

namespace webapps {
enum class InstallResultCode;
enum class InstallableStatusCode;
}  // namespace webapps
namespace web_app {
class AppLock;
struct WebAppInstallInfo;
class WebAppDataRetriever;
class WebAppProvider;
class WebInstallManifestFetcher;
enum class WebInstallManifestFetchError;

// Result codes for Web Install API results.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(WebInstallServiceResult)
enum class WebInstallServiceResult {
  kSuccess = 0,
  kSuccessAlreadyInstalled = 1,
  kUnexpectedFailure = 2,
  kPermissionDenied = 3,
  kUnsupportedProfile = 4,
  kCanceledByUser = 5,
  kInstallCommandFailed = 6,
  kNoCustomManifestId = 7,
  kManifestIdMismatch = 8,
  kInstallInProgress = 9,
  // Insert new values above this line.
  kMaxValue = kInstallInProgress,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/webapps/enums.xml:WebInstallServiceResult)

// Install types for the Web Install API.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(WebInstallServiceType)
enum class WebInstallServiceType {
  kCurrentDocument = 0,
  kBackgroundDocument = 1,
  // Insert new values above this line.
  kMaxValue = kBackgroundDocument,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/webapps/enums.xml:WebInstallServiceType)

// Used to coordinate running the `InstallCallback` from `Install()` with firing
// the appropriate Uma result.
using InstallCallbackWithMetrics =
    base::OnceCallback<void(web_app::WebInstallServiceResult,
                            blink::mojom::WebInstallServiceResult,
                            std::optional<webapps::ManifestId>)>;

// Wraps the `InstallFromManifestCallback` so that the install-in-progress guard
// is automatically released on every exit path.
using InstallFromManifestCallbackWithGuard =
    base::OnceCallback<void(blink::mojom::WebInstallServiceResult)>;

// Service side implementation for the Blink Web Install API. Takes the
// parameters from the API call in the form of `InstallOptionsPtr`, and decides
// whether to install the current document or a background document.
// Background document installs will prompt for approval/denial of the Web app
// installation permission for the calling origin.
class WebInstallServiceImpl
    : public content::DocumentService<blink::mojom::WebInstallService> {
 public:
  WebInstallServiceImpl(const WebInstallServiceImpl&) = delete;
  WebInstallServiceImpl& operator=(const WebInstallServiceImpl&) = delete;

  static void CreateIfAllowed(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::WebInstallService> receiver);

  // Test-only overrides for rate limiting constants.
  static base::AutoReset<size_t> SetMaxCrossOriginQueriesForTesting(
      size_t max_queries);
  static base::AutoReset<base::TimeDelta>
  SetMinCrossOriginQueryIntervalForTesting(base::TimeDelta interval);

  // blink::mojom::WebInstallService implementation:
  void IsInstalled(blink::mojom::InstallOptionsPtr options,
                   IsInstalledCallback callback) override;
  // TODO(crbug.com/520025525): Remove install_url code.
  void Install(blink::mojom::InstallOptionsPtr options,
               InstallCallback callback) override;
  void InstallFromElement(blink::mojom::InstallOptionsPtr options,
                          InstallCallback callback) override;
  void InstallFromManifest(blink::mojom::ManifestInstallOptionsPtr options,
                           InstallFromManifestCallback callback) override;
  void ElementInstallFromManifest(
      blink::mojom::ManifestInstallOptionsPtr options,
      InstallFromManifestCallback callback) override;

 private:
  // Shared implementation for Install() and InstallFromElement().
  // `triggered_from_element` controls metrics routing and whether the
  // permission prompt is bypassed (the <install> element handles permission
  // via its own UI).
  void InstallInternal(blink::mojom::InstallOptionsPtr options,
                       InstallCallback callback,
                       bool triggered_from_element);

  // Internal entry point for the manifest URL install flow. Acquires the
  // install-in-progress guard and wraps the callback so the guard is
  // released on every exit path.
  void InstallFromManifestInternal(
      blink::mojom::ManifestInstallOptionsPtr options,
      InstallFromManifestCallback callback,
      bool triggered_from_element);

  WebInstallServiceImpl(
      content::RenderFrameHost& render_frame_host,
      mojo::PendingReceiver<blink::mojom::WebInstallService> receiver);
  ~WebInstallServiceImpl() override;

  // Manages `install_in_progress_`.
  bool IsInstallInProgress() const;
  base::ScopedClosureRunner ReserveInstallInProgress();
  void ReleaseInstallInProgress();

  void OnInstallNotSupportedDialogClosed(
      InstallCallbackWithMetrics callback_with_metrics);

  void TryInstallCurrentDocument(
      InstallCallbackWithMetrics callback_with_metrics);

  void CheckForInstalledAppMaybeLaunch(
      content::WebContents* web_contents,
      InstallCallbackWithMetrics callback_with_metrics,
      AppLock& lock,
      base::DictValue& debug_value);

  void OnIntentPickerMaybeLaunched(
      InstallCallbackWithMetrics callback_with_metrics,
      webapps::AppId app_id,
      bool user_chose_to_open);

  void OnGotManifestForCurrentDocumentInstall(
      InstallCallbackWithMetrics callback_with_metrics,
      WebAppProvider* provider,
      base::WeakPtr<WebAppDataRetriever> data_retriever,
      const base::expected<blink::mojom::ManifestPtr,
                           blink::mojom::RequestManifestErrorPtr>& result);

  void RequestWebInstallPermission(
      base::OnceCallback<void(const std::vector<content::PermissionResult>&)>
          callback);

  void OnPermissionDecided(
      blink::mojom::InstallOptionsPtr install_options,
      InstallCallbackWithMetrics callback_with_metrics,
      const std::vector<content::PermissionResult>& permission_result);

  // `install_info` was fetched from an install url and is used to populate the
  // background launch dialog.
  void OnInstallInfoFromInstallUrlFetched(
      InstallCallbackWithMetrics callback_with_metrics,
      webapps::AppId app_id,
      const GURL& manifest_id,
      std::unique_ptr<WebAppInstallInfo> install_info);

  // Triggers the icon launch dialog after any behavior has been applied on the
  // icon, like masking.
  void OnIconFinalizedTriggerDialog(
      InstallCallbackWithMetrics callback_with_metrics,
      webapps::AppId app_id,
      const GURL& manifest_id,
      std::u16string app_title,
      const SkBitmap icon_to_use);

  // Used by the launch dialog to report whether the user accepted the launch.
  void OnBackgroundAppLaunchDialogClosed(
      InstallCallbackWithMetrics callback_with_metrics,
      const GURL& manifest_id,
      bool accepted);

  // Used by web app install dialog code as the WebAppInstalledCallback.
  // Reports install success or failure back to Blink via `callback`.
  void OnAppInstalled(InstallCallbackWithMetrics callback_with_metrics,
                      const webapps::AppId& app_id,
                      webapps::InstallResultCode code);

  // Runs the actual registrar lookup for `IsInstalled` and replies via
  // `callback`. Posted with a delay for cross-origin queries, and invoked
  // synchronously for same-origin queries.
  void RunIsInstalledLookup(GURL install_target,
                            std::optional<GURL> manifest_id,
                            IsInstalledCallback callback);

  // Callback for when InstallFromManifest's fetch completes.
  void OnManifestFetched(
      InstallFromManifestCallbackWithGuard callback_with_guard,
      blink::mojom::ManifestInstallOptionsPtr options,
      bool triggered_from_element,
      base::expected<std::string, WebInstallManifestFetchError> result);

  // Callback for when the manifest parse command completes.
  void OnManifestParsed(
      InstallFromManifestCallbackWithGuard callback_with_guard,
      blink::mojom::ManifestInstallOptionsPtr options,
      bool triggered_from_element,
      blink::mojom::ManifestPtr manifest);

  void OnManifestInstallNotSupportedDialogClosed(
      InstallFromManifestCallbackWithGuard callback_with_guard);

  // Callback for when the manifest URL permission prompt completes.
  void OnManifestPermissionDecided(
      InstallFromManifestCallbackWithGuard callback_with_guard,
      blink::mojom::ManifestInstallOptionsPtr options,
      const std::vector<content::PermissionResult>& permission_result);

  // Shared "permission granted, proceed to install" step for the manifest URL
  // flow.
  void ContinueManifestInstall(
      InstallFromManifestCallbackWithGuard callback_with_guard,
      blink::mojom::ManifestInstallOptionsPtr options);

  // Only one install can be in progress at a time.
  bool install_in_progress_ = false;

  const content::GlobalRenderFrameHostId frame_routing_id_;
  GURL last_committed_url_;
  // Active data retrievers. They are destroyed when this service is destroyed
  // or when their callback completes.
  absl::flat_hash_set<std::unique_ptr<WebAppDataRetriever>> data_retrievers_;

  // Running count of cross-origin query attempts (not accepts).
  size_t cross_origin_query_count_ = 0;

  // The earliest TimeTicks at which the next cross-origin lookup is allowed to
  // run; it advances monotonically each time a lookup is scheduled, paced by
  // `g_min_cross_origin_query_interval`.
  base::TimeTicks next_cross_origin_query_dispatch_time_;

  // Active manifest fetcher for InstallFromManifest. Destroyed when the
  // fetch completes or this service is destroyed.
  std::unique_ptr<WebInstallManifestFetcher> manifest_fetcher_;

  base::WeakPtrFactory<web_app::WebInstallServiceImpl> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_INSTALL_SERVICE_IMPL_H_
