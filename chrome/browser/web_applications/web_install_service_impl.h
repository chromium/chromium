// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_INSTALL_SERVICE_IMPL_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_INSTALL_SERVICE_IMPL_H_

#include <optional>
#include <vector>

#include "base/memory/weak_ptr.h"
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
  // Insert new values above this line.
  kMaxValue = kManifestIdMismatch,
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
                            webapps::ManifestId)>;

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

  // blink::mojom::WebInstallService implementation:
  void IsInstalled(blink::mojom::InstallOptionsPtr options,
                   IsInstalledCallback callback) override;
  void Install(blink::mojom::InstallOptionsPtr options,
               InstallCallback callback) override;
  void InstallFromElement(blink::mojom::InstallOptionsPtr options,
                          InstallCallback callback) override;

 private:
  WebInstallServiceImpl(
      content::RenderFrameHost& render_frame_host,
      mojo::PendingReceiver<blink::mojom::WebInstallService> receiver);
  ~WebInstallServiceImpl() override;

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

  // The original parameters received from the blink sides - a required
  // `install_url` and an optional `manifest_id`.
  blink::mojom::InstallOptionsPtr install_options_;
  const content::GlobalRenderFrameHostId frame_routing_id_;
  GURL last_committed_url_;
  // True if install was triggered from <install> element rather than JS API.
  bool triggered_from_element_ = false;
  // Active data retrievers. They are destroyed when this service is destroyed
  // or when their callback completes.
  absl::flat_hash_set<std::unique_ptr<WebAppDataRetriever>> data_retrievers_;

  base::WeakPtrFactory<web_app::WebInstallServiceImpl> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_INSTALL_SERVICE_IMPL_H_
