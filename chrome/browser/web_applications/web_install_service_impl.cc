// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_install_service_impl.h"

#include <optional>
#include <vector>

#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/commands/web_install_from_url_command.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"
#include "chrome/browser/web_applications/web_contents/web_contents_manager.h"
#include "components/permissions/permission_request.h"
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
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"
#include "third_party/blink/public/mojom/web_install/web_install.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace web_app {

namespace {

using PermissionStatus = blink::mojom::PermissionStatus;

// Checks if an app is installed based on `manifest_id`, if possible. Otherwise
// falls back to `install_target`. Used by the background doc install path.
// These are allowed to use unsafe registrar accesses, as this is the first step
// in a launch flow, and we later queue a command to launch, which will safely
// recheck the app's state in the registrar, and fail gracefully if it's no
// longer installed.
std::optional<webapps::AppId> IsAppInstalled(
    Profile* profile,
    const GURL& install_target,
    const std::optional<GURL>& manifest_id) {
  auto* provider = WebAppProvider::GetForWebApps(profile);
  CHECK(provider);

  // Only consider apps that launch in a standalone window, or were installed
  // by the user.
  WebAppFilter filter = WebAppFilter::LaunchableFromInstallApi();

  // If the developer provided a manifest ID, use it to look up the app. This
  // avoids issues with nested app scopes and `install_target` potentially
  // launching the wrong app.
  if (manifest_id) {
    webapps::AppId app_id_from_manifest_id =
        GenerateAppIdFromManifestId(manifest_id.value());

    bool found_app = provider->registrar_unsafe().AppMatches(
        app_id_from_manifest_id, filter);

    return found_app ? std::optional<webapps::AppId>(app_id_from_manifest_id)
                     : std::nullopt;
  }

  // No `manifest_id` was provided. Check for the app by `install_target`. This
  // is less accurate and may result in another app being launched.
  return provider->registrar_unsafe().FindBestAppWithUrlInScope(install_target,
                                                                filter);
}

}  // namespace

WebInstallServiceImpl::WebInstallServiceImpl(
    content::RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<blink::mojom::WebInstallService> receiver)
    : content::DocumentService<blink::mojom::WebInstallService>(
          render_frame_host,
          std::move(receiver)),
      frame_routing_id_(render_frame_host.GetGlobalId()) {}

WebInstallServiceImpl::~WebInstallServiceImpl() = default;

// static
void WebInstallServiceImpl::CreateIfAllowed(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::WebInstallService> receiver) {
  CHECK(render_frame_host);

  // This class is created only on the primary main frame.
  if (!render_frame_host->IsInPrimaryMainFrame()) {
    receiver.reset();
    return;
  }

  // TODO(crbug.com/402547563): Installing web apps is not supported from
  // off-the-record profiles.
  // This check stops the ServiceImpl from being
  // created in Incognito mode. (To exclude Guest mode too, switch to
  // AreWebAppsUserInstallable). It may need to be removed depending where the
  // auto rejection is implemented.
  if (!AreWebAppsEnabled(Profile::FromBrowserContext(
          content::WebContents::FromRenderFrameHost(render_frame_host)
              ->GetBrowserContext()))) {
    receiver.reset();
    return;
  }

  if (!render_frame_host->GetLastCommittedURL().SchemeIsHTTPOrHTTPS()) {
    receiver.reset();
    return;
  }

  new WebInstallServiceImpl(*render_frame_host, std::move(receiver));
}

void WebInstallServiceImpl::Install(blink::mojom::InstallOptionsPtr options,
                                    InstallCallback callback) {
  GURL install_target;
  const GURL current_url = render_frame_host().GetLastCommittedURL();

  // `options` is null if the 0-parameter signature was called.
  if (!options) {
    // No parameters means we want to install the current document.
    install_target = current_url;
  } else {
    install_target = GURL(options->install_url);
  }
  install_options_ = std::move(options);

  // Do not allow installation of file:// or chrome:// urls.
  if (!install_target.SchemeIsHTTPOrHTTPS()) {
    std::move(callback).Run(blink::mojom::WebInstallServiceResult::kAbortError,
                            GURL());
    return;
  }

  // TODO(crbug.com/402547563): Installing web apps is not supported from
  // off-the-record profiles.

  // Initiate installation of the current document.
  // TODO(crbug.com/407473727): Treat install(self) and install(self, self) as
  // background installs, but skip the permissions checking code. Tests will
  // also likely need updating.
  if (install_target == current_url) {
    TryInstallCurrentDocument(std::move(callback));

    // Current document installs don't require the permissions checking code.
    return;
  }

  auto* rfh = content::RenderFrameHost::FromID(frame_routing_id_);
  if (!rfh) {
    std::move(callback).Run(blink::mojom::WebInstallServiceResult::kAbortError,
                            GURL());
    return;
  }

  // Verify that the calling app has the Web Install permissions policy set.
  if (!rfh->IsFeatureEnabled(
          network::mojom::PermissionsPolicyFeature::kWebAppInstallation)) {
    std::move(callback).Run(blink::mojom::WebInstallServiceResult::kAbortError,
                            GURL());
    return;
  }

  RequestWebInstallPermission(
      base::BindOnce(&WebInstallServiceImpl::OnPermissionDecided,
                     weak_ptr_factory_.GetWeakPtr(), install_target,
                     install_options_->manifest_id, std::move(callback)));
}

void WebInstallServiceImpl::TryInstallCurrentDocument(
    InstallCallback callback) {
  auto* web_contents =
      content::WebContents::FromRenderFrameHost(&render_frame_host());
  auto* provider = WebAppProvider::GetForWebContents(web_contents);
  // TODO(crbug.com/402547563): Installing web apps is not supported from
  // off-the-record profiles.
  // As of now, WebInstallServiceImpl is only created if `AreWebAppsEnabled` for
  // the current browsing context (see `CreateIfAllowed`), so the provider
  // should always be available. If this changes, this check can be
  // reevaluated.
  CHECK(provider);

  // Check if the current document is already installed.
  const webapps::AppId* app_id =
      web_app::WebAppTabHelper::GetAppId(web_contents);
  if (!app_id) {
    // The current document is not installed yet. Retrieve the manifest to
    // perform id validation checks.
    std::unique_ptr<WebAppDataRetriever> data_retriever =
        provider->web_contents_manager().CreateDataRetriever();
    webapps::InstallableParams params;
    params.installable_criteria =
        webapps::InstallableCriteria::kValidManifestWithIcons;
    data_retriever->CheckInstallabilityAndRetrieveManifest(
        web_contents,
        base::BindOnce(&WebInstallServiceImpl::
                           OnDidRetrieveManifestForCurrentDocumentInstall,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                       provider),
        params);
    return;
  }
  // If the current document that is trying to install is currently in a PWA
  // window, return kSuccessAlreadyInstalled.
  web_app::WebAppTabHelper* tab_helper =
      web_app::WebAppTabHelper::FromWebContents(web_contents);
  if (tab_helper->is_in_app_window()) {
    OnAppInstalled(std::move(callback), *app_id,
                   webapps::InstallResultCode::kSuccessAlreadyInstalled);
    return;
  }

  provider->scheduler().ScheduleCallback<AppLock>(
      "WebInstallServiceImpl::TryInstallCurrentDocument",
      AppLockDescription(*app_id),
      base::BindOnce(&WebInstallServiceImpl::CheckForInstalledAppMaybeLaunch,
                     weak_ptr_factory_.GetWeakPtr(), web_contents,
                     std::move(callback)),
      /*on_complete=*/base::DoNothing());
}

void WebInstallServiceImpl::CheckForInstalledAppMaybeLaunch(
    content::WebContents* web_contents,
    InstallCallback callback,
    AppLock& lock,
    base::Value::Dict& debug_value) {
  // Now that we've locked the app, re-confirm the current document is still
  // already installed on the current device.
  const webapps::AppId* app_id =
      web_app::WebAppTabHelper::GetAppId(web_contents);
  if (!app_id || !lock.registrar().AppMatches(
                     *app_id, web_app::WebAppFilter::InstalledInChrome())) {
    std::move(callback).Run(blink::mojom::WebInstallServiceResult::kAbortError,
                            GURL());
    return;
  }

  auto* provider = WebAppProvider::GetForWebContents(web_contents);
  CHECK(provider);

  // Now that we know the app is already installed, show the intent picker.
  provider->ui_manager().ShowIntentPicker(
      web_contents->GetURL(), web_contents,
      base::BindOnce(&WebInstallServiceImpl::OnIntentPickerMaybeLaunched,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     *app_id));
}

void WebInstallServiceImpl::OnIntentPickerMaybeLaunched(
    InstallCallback callback,
    webapps::AppId app_id,
    bool user_chose_to_open) {
  // If the user chose to open the app in the intent picker, return success.
  // Otherwise, return an abort error.
  if (user_chose_to_open) {
    OnAppInstalled(std::move(callback), app_id,
                   webapps::InstallResultCode::kSuccessAlreadyInstalled);
  } else {
    std::move(callback).Run(blink::mojom::WebInstallServiceResult::kAbortError,
                            GURL());
  }
}

void WebInstallServiceImpl::OnDidRetrieveManifestForCurrentDocumentInstall(
    InstallCallback callback,
    WebAppProvider* provider,
    blink::mojom::ManifestPtr opt_manifest,
    bool valid_manifest_for_web_app,
    webapps::InstallableStatusCode error_code) {
  // If for some reason a valid manifest was not found, cancel with the
  // generic abort error.
  if (!opt_manifest || !valid_manifest_for_web_app) {
    std::move(callback).Run(blink::mojom::WebInstallServiceResult::kAbortError,
                            GURL());
    return;
  }
  // Ensure that the manifest is from the same trusted origin as the current
  // document.
  if (!origin().IsSameOriginWith(opt_manifest->id)) {
    std::move(callback).Run(blink::mojom::WebInstallServiceResult::kAbortError,
                            GURL());
    return;
  }

  // The manifest must have a developer-specified id if navigator.install was
  // invoked without a `manifest_id` (ie. the 0 or 1 parameter version).
  bool manifest_must_have_id =
      !install_options_ || !install_options_->manifest_id;
  if (manifest_must_have_id && !opt_manifest->has_custom_id) {
    std::move(callback).Run(blink::mojom::WebInstallServiceResult::kDataError,
                            GURL());
    return;
  }
  // navigator.install was invoked with a manifest_id, so the current document
  // is not required to have a developer-specified id. However, the id passed
  // to navigator.install must match the current document's computed id.
  if (!manifest_must_have_id &&
      install_options_->manifest_id.value() != opt_manifest->id) {
    std::move(callback).Run(blink::mojom::WebInstallServiceResult::kDataError,
                            GURL());
    return;
  }

  provider->ui_manager().TriggerInstallDialog(
      content::WebContents::FromRenderFrameHost(&render_frame_host()),
      webapps::WebappInstallSource::WEB_INSTALL,
      base::BindOnce(&WebInstallServiceImpl::OnAppInstalled,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebInstallServiceImpl::RequestWebInstallPermission(
    base::OnceCallback<void(const std::vector<PermissionStatus>&)> callback) {
  content::BrowserContext* const browser_context =
      render_frame_host().GetBrowserContext();
  if (!browser_context) {
    // TODO(crbug.com/381282538): Technically this isn't correct since
    // permission wasn't denied. Same for the if check below. Update to a more
    // appropriate error.
    std::move(callback).Run(
        std::vector<PermissionStatus>({PermissionStatus::DENIED}));
    return;
  }

  content::PermissionController* const permission_controller =
      browser_context->GetPermissionController();
  if (!permission_controller) {
    std::move(callback).Run(
        std::vector<PermissionStatus>({PermissionStatus::DENIED}));
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
          std::vector<PermissionStatus>({PermissionStatus::GRANTED}));
      return;
    case PermissionStatus::DENIED:
      std::move(callback).Run(
          std::vector<PermissionStatus>({PermissionStatus::DENIED}));
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
    const GURL& install_target,
    const std::optional<GURL>& manifest_id,
    InstallCallback callback,
    const std::vector<PermissionStatus>& permission_status) {
  CHECK_EQ(permission_status.size(), 1u);
  if (permission_status[0] != PermissionStatus::GRANTED) {
    std::move(callback).Run(blink::mojom::WebInstallServiceResult::kAbortError,
                            GURL());
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
    std::move(callback).Run(blink::mojom::WebInstallServiceResult::kAbortError,
                            GURL());
    return;
  }

  auto* profile =
      Profile::FromBrowserContext(render_frame_host().GetBrowserContext());
  auto* provider = WebAppProvider::GetForWebApps(profile);
  CHECK(provider);
  if (provider->command_manager().IsInstallingForWebContents(web_contents)) {
    // Another install is already scheduled on the current web contents.
    // Cancel this background install/launch flow.
    std::move(callback).Run(blink::mojom::WebInstallServiceResult::kAbortError,
                            GURL());
    return;
  }

  // Check if the background document is already installed so we can show the
  // launch dialog instead of the install dialog.
  std::optional<webapps::AppId> app_id =
      IsAppInstalled(profile, install_target, manifest_id);
  if (app_id) {
    // See `IsAppInstalled` for why these are unsafe accesses.
    const GURL& installed_manifest_id =
        provider->registrar_unsafe().GetComputedManifestId(app_id.value());
    CHECK(installed_manifest_id != GURL());

    // Name to display in the dialog.
    std::string app_name =
        provider->registrar_unsafe().GetAppShortName(app_id.value());
    // TODO(crbug.com/422940463): Show app icon in new launch dialog for
    // background document launches.

    provider->ui_manager().TriggerLaunchDialogForBackgroundInstall(
        web_contents, app_id.value(), profile, app_name,
        base::BindOnce(
            &WebInstallServiceImpl::OnBackgroundAppLaunchDialogClosed,
            weak_ptr_factory_.GetWeakPtr(), std::move(callback),
            installed_manifest_id));
    return;
  }

  // `install_target` was not installed locally with OS integration. Proceed
  // with the background install.

  // Register the background install on the current web contents.
  std::unique_ptr<webapps::MlInstallOperationTracker> install_tracker =
      promoter->RegisterCurrentInstallForWebContents(
          webapps::WebappInstallSource::WEB_INSTALL);

  provider->ui_manager().TriggerInstallDialogForBackgroundInstall(
      web_contents, std::move(install_tracker), install_target, manifest_id,
      base::BindOnce(&WebInstallServiceImpl::OnAppInstalled,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebInstallServiceImpl::OnBackgroundAppLaunchDialogClosed(
    InstallCallback callback,
    const GURL& manifest_id,
    bool accepted) {
  // For privacy reasons, only resolve with success if the user accepted.
  std::move(callback).Run(
      accepted ? blink::mojom::WebInstallServiceResult::kSuccess
               : blink::mojom::WebInstallServiceResult::kAbortError,
      accepted ? manifest_id : GURL());
}

void WebInstallServiceImpl::OnAppInstalled(InstallCallback callback,
                                           const webapps::AppId& app_id,
                                           webapps::InstallResultCode code) {
  // Results to report for generic failures.
  blink::mojom::WebInstallServiceResult install_result =
      blink::mojom::WebInstallServiceResult::kAbortError;
  webapps::ManifestId manifest_id_result;

  if (webapps::IsSuccess(code)) {
    install_result = blink::mojom::WebInstallServiceResult::kSuccess;

    auto* profile =
        Profile::FromBrowserContext(render_frame_host().GetBrowserContext());
    auto* provider = WebAppProvider::GetForWebApps(profile);
    CHECK(provider);

    manifest_id_result =
        provider->registrar_unsafe().GetComputedManifestId(app_id);
    CHECK(!manifest_id_result.is_empty());
  } else if (code == webapps::InstallResultCode::kNoCustomManifestId ||
             code == webapps::InstallResultCode::kManifestIdMismatch) {
    install_result = blink::mojom::WebInstallServiceResult::kDataError;
  }

  std::move(callback).Run(install_result, manifest_id_result);
}

}  // namespace web_app
