// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/sub_apps_service_impl.h"

#include <string>
#include <utility>

#include "base/barrier_callback.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/types/expected.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"
#include "url/origin.h"

using blink::mojom::SubAppsService;
using blink::mojom::SubAppsServiceAddParametersPtr;
using blink::mojom::SubAppsServiceAddResult;
using blink::mojom::SubAppsServiceAddResultPtr;
using blink::mojom::SubAppsServiceListResult;
using blink::mojom::SubAppsServiceListResultEntry;
using blink::mojom::SubAppsServiceListResultEntryPtr;
using blink::mojom::SubAppsServiceRemoveResult;
using blink::mojom::SubAppsServiceRemoveResultPtr;
using blink::mojom::SubAppsServiceResultCode;

namespace web_app {

namespace {

// Resolve string `path` with `origin`, and if the resulting GURL isn't same
// origin with `origin` then return an error (for which the caller needs to
// raise a `ReportBadMessageAndDeleteThis`).
base::expected<GURL, std::string> ConvertPathToUrl(const std::string& path,
                                                   const url::Origin& origin) {
  GURL resolved = origin.GetURL().Resolve(path);

  if (!origin.IsSameOriginWith(resolved)) {
    return base::unexpected(
        "SubAppsServiceImpl: Different origin arg to that of the calling app.");
  }

  if (resolved.is_empty()) {
    return base::unexpected("SubAppsServiceImpl: Empty url.");
  }

  if (!resolved.is_valid()) {
    return base::unexpected("SubAppsServiceImpl: Invalid url.");
  }

  return base::ok(resolved);
}

std::string ConvertUrlToPath(const ManifestId& manifest_id) {
  return manifest_id.PathForRequest();
}

base::expected<std::vector<std::pair<ManifestId, GURL>>, std::string>
AddOptionsFromMojo(
    const url::Origin& origin,
    const std::vector<SubAppsServiceAddParametersPtr>& sub_apps_to_add_mojo) {
  std::vector<std::pair<ManifestId, GURL>> sub_apps;
  for (const auto& sub_app : sub_apps_to_add_mojo) {
    base::expected<ManifestId, std::string> manifest_id =
        ConvertPathToUrl(sub_app->manifest_id_path, origin);
    if (!manifest_id.has_value()) {
      return base::unexpected(manifest_id.error());
    }
    base::expected<GURL, std::string> install_url =
        ConvertPathToUrl(sub_app->install_url_path, origin);
    if (!install_url.has_value()) {
      return base::unexpected(install_url.error());
    }
    sub_apps.emplace_back(manifest_id.value(), install_url.value());
  }
  return sub_apps;
}

WebAppProvider* GetWebAppProvider(content::RenderFrameHost& render_frame_host) {
  auto* const initiator_web_contents =
      content::WebContents::FromRenderFrameHost(&render_frame_host);
  auto* provider = WebAppProvider::GetForWebContents(initiator_web_contents);
  DCHECK(provider);
  return provider;
}

const AppId* GetAppId(content::RenderFrameHost& render_frame_host) {
  auto* const initiator_web_contents =
      content::WebContents::FromRenderFrameHost(&render_frame_host);
  return WebAppTabHelper::GetAppId(initiator_web_contents);
}

blink::mojom::SubAppsServiceResultCode InstallResultCodeToMojo(
    webapps::InstallResultCode install_result_code) {
  return webapps::IsSuccess(install_result_code)
             ? blink::mojom::SubAppsServiceResultCode::kSuccess
             : blink::mojom::SubAppsServiceResultCode::kFailure;
}

}  // namespace

SubAppsServiceImpl::SubAppsServiceImpl(
    content::RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<SubAppsService> receiver)
    : DocumentService(render_frame_host, std::move(receiver)) {}

SubAppsServiceImpl::~SubAppsServiceImpl() = default;

SubAppsServiceImpl::AddCallInfo::AddCallInfo() = default;
SubAppsServiceImpl::AddCallInfo::~AddCallInfo() = default;

// static
void SubAppsServiceImpl::CreateIfAllowed(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<SubAppsService> receiver) {
  CHECK(render_frame_host);

  // This class is created only on the primary main frame.
  if (!render_frame_host->IsInPrimaryMainFrame()) {
    return;
  }

  // Bail if Web Apps aren't enabled on current profile.
  if (!AreWebAppsEnabled(Profile::FromBrowserContext(
          content::WebContents::FromRenderFrameHost(render_frame_host)
              ->GetBrowserContext()))) {
    return;
  }

  // The object is bound to the lifetime of `render_frame_host` and the mojo
  // connection. See DocumentService for details.
  new SubAppsServiceImpl(*render_frame_host, std::move(receiver));
}

void SubAppsServiceImpl::Add(
    std::vector<SubAppsServiceAddParametersPtr> sub_apps_to_add,
    AddCallback result_callback) {
  WebAppProvider* provider = GetWebAppProvider(render_frame_host());
  if (!provider->on_registry_ready().is_signaled()) {
    provider->on_registry_ready().Post(
        FROM_HERE,
        base::BindOnce(&SubAppsServiceImpl::Add, weak_ptr_factory_.GetWeakPtr(),
                       std::move(sub_apps_to_add), std::move(result_callback)));
    return;
  }

  const AppId* parent_app_id = GetAppId(render_frame_host());
  // Verify that the calling app is installed itself and is not a sub app
  // itself. This check is done here and not in `CreateIfAllowed` because of a
  // potential race between doing the check there and then running the current
  // function, and the parent app being installed/uninstalled.
  if (!parent_app_id || provider->registrar_unsafe()
                            .GetAppById(*parent_app_id)
                            ->IsSubAppInstalledApp()) {
    std::vector<SubAppsServiceAddResultPtr> result;
    for (const auto& sub_app : sub_apps_to_add) {
      result.emplace_back(SubAppsServiceAddResult::New(
          sub_app->manifest_id_path, SubAppsServiceResultCode::kFailure));
    }
    std::move(result_callback).Run(/*mojom_results=*/std::move(result));
    return;
  }

  if (sub_apps_to_add.empty()) {
    std::move(result_callback).Run(/*mojom_results=*/{});
    return;
  }

  base::expected<std::vector<std::pair<ManifestId, GURL>>, std::string>
      add_options = AddOptionsFromMojo(
          render_frame_host().GetLastCommittedOrigin(), sub_apps_to_add);
  if (!add_options.has_value()) {
    // Compromised renderer, bail immediately (this call deletes *this).
    return ReportBadMessageAndDeleteThis(add_options.error());
  }

  CHECK(AreWebAppsUserInstallable(
      Profile::FromBrowserContext(render_frame_host().GetBrowserContext())));

  // Assign id to this add call
  int add_call_id = next_add_call_id_++;
  AddCallInfo& add_call_info = add_call_info_[add_call_id];
  add_call_info.mojo_callback = std::move(result_callback);

  CollectInstallData(add_call_id, add_options.value());
}

void SubAppsServiceImpl::CollectInstallData(
    int add_call_id,
    std::vector<std::pair<ManifestId, GURL>> requested_installs) {
  const auto install_info_collector = base::BarrierCallback<
      std::pair<ManifestId, std::unique_ptr<WebAppInstallInfo>>>(
      requested_installs.size(),
      base::BindOnce(&SubAppsServiceImpl::ProcessInstallData,
                     weak_ptr_factory_.GetWeakPtr(), add_call_id));

  WebAppProvider* provider = GetWebAppProvider(render_frame_host());
  // Schedule data collection for each requested install
  for (const auto& [manifest_id, url_to_load] : requested_installs) {
    // Check if app is already installed as a sub app
    if (provider->registrar_unsafe().WasInstalledBySubApp(
            GenerateAppIdFromManifestId(manifest_id))) {
      add_call_info_[add_call_id].results.emplace_back(
          SubAppsServiceAddResult::New(
              ConvertUrlToPath(manifest_id),
              blink::mojom::SubAppsServiceResultCode::kSuccess));
      install_info_collector.Run(std::pair(GURL(), nullptr));
      continue;
    }

    provider->scheduler().FetchInstallInfoFromInstallUrl(
        manifest_id, url_to_load,
        base::BindOnce(
            [](ManifestId manifest_app_id,
               std::unique_ptr<WebAppInstallInfo> install_info) {
              return std::pair(manifest_app_id, std::move(install_info));
            },
            manifest_id)
            .Then(install_info_collector));
  }
}

void SubAppsServiceImpl::ProcessInstallData(
    int add_call_id,
    std::vector<std::pair<ManifestId, std::unique_ptr<WebAppInstallInfo>>>
        install_data) {
  AddCallInfo& add_call_info = add_call_info_[add_call_id];
  const AppId* parent_app_id = GetAppId(render_frame_host());

  for (auto& [manifest_id, install_info] : install_data) {
    // If manifest_id is empty, the app was already installed and no install
    // info was collected. If it is invalid, do not to trigger an installation
    // since the command will run into problems.
    if (!manifest_id.is_valid()) {
      continue;
    }

    if (install_info) {
      install_info->parent_app_id = *parent_app_id;
      install_info->user_display_mode = mojom::UserDisplayMode::kStandalone;
      add_call_info.install_infos.emplace_back(std::move(install_info));
    } else {
      // Log error if install info could not be loaded
      add_call_info.results.emplace_back(SubAppsServiceAddResult::New(
          ConvertUrlToPath(manifest_id),
          blink::mojom::SubAppsServiceResultCode::kFailure));
    }
  }

  if (add_call_info.install_infos.empty()) {
    FinishAddCall(add_call_id, {});
    return;
  }

  ScheduleSubAppInstalls(add_call_id);
}

void SubAppsServiceImpl::ScheduleSubAppInstalls(int add_call_id) {
  AddCallInfo& add_call_info = add_call_info_[add_call_id];

  const auto install_results_collector = base::BarrierCallback<
      std::tuple<ManifestId, AppId, webapps::InstallResultCode>>(
      add_call_info.install_infos.size(),
      base::BindOnce(&SubAppsServiceImpl::FinishAddCall,
                     weak_ptr_factory_.GetWeakPtr(), add_call_id));

  // Schedule install for each install_info that was collected
  WebAppProvider* provider = GetWebAppProvider(render_frame_host());
  for (auto& install_info : add_call_info.install_infos) {
    ManifestId manifest_id = install_info->manifest_id;
    provider->scheduler().InstallFromInfo(
        std::move(install_info), /*overwrite_existing_manifest_fields=*/false,
        webapps::WebappInstallSource::SUB_APP,
        base::BindOnce(
            [](ManifestId manifest_id, const AppId& app_id,
               webapps::InstallResultCode result_code) {
              return std::tuple(manifest_id, app_id, result_code);
            },
            manifest_id)
            .Then(install_results_collector));
  }
}

void SubAppsServiceImpl::FinishAddCall(
    int add_call_id,
    std::vector<std::tuple<ManifestId, AppId, webapps::InstallResultCode>>
        install_results) {
  AddCallInfo& add_call_info = add_call_info_[add_call_id];

  for (const auto& [manifest_id, app_id, result_code] : install_results) {
    add_call_info.results.emplace_back(SubAppsServiceAddResult::New(
        ConvertUrlToPath(manifest_id), InstallResultCodeToMojo(result_code)));
  }

  std::move(add_call_info.mojo_callback).Run(std::move(add_call_info.results));

  add_call_info_.erase(add_call_id);
}

void SubAppsServiceImpl::List(ListCallback result_callback) {
  WebAppProvider* provider = GetWebAppProvider(render_frame_host());
  if (!provider->on_registry_ready().is_signaled()) {
    provider->on_registry_ready().Post(
        FROM_HERE, base::BindOnce(&SubAppsServiceImpl::List,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  std::move(result_callback)));
    return;
  }

  // Verify that the calling app is installed itself (cf. `Add`).
  const AppId* parent_app_id = GetAppId(render_frame_host());
  if (!parent_app_id) {
    return std::move(result_callback)
        .Run(SubAppsServiceListResult::New(
            SubAppsServiceResultCode::kFailure,
            std::vector<SubAppsServiceListResultEntryPtr>()));
  }

  WebAppRegistrar& registrar = provider->registrar_unsafe();

  std::vector<SubAppsServiceListResultEntryPtr> sub_apps_list;
  for (const AppId& sub_app_id : registrar.GetAllSubAppIds(*parent_app_id)) {
    const WebApp* sub_app = registrar.GetAppById(sub_app_id);
    ManifestId manifest_id = sub_app->manifest_id();
    sub_apps_list.push_back(SubAppsServiceListResultEntry::New(
        ConvertUrlToPath(manifest_id), sub_app->untranslated_name()));
  }

  std::move(result_callback)
      .Run(SubAppsServiceListResult::New(SubAppsServiceResultCode::kSuccess,
                                         std::move(sub_apps_list)));
}

void SubAppsServiceImpl::Remove(
    const std::vector<std::string>& manifest_id_paths,
    RemoveCallback result_callback) {
  WebAppProvider* provider = GetWebAppProvider(render_frame_host());
  if (!provider->on_registry_ready().is_signaled()) {
    provider->on_registry_ready().Post(
        FROM_HERE,
        base::BindOnce(&SubAppsServiceImpl::Remove,
                       weak_ptr_factory_.GetWeakPtr(), manifest_id_paths,
                       std::move(result_callback)));
    return;
  }

  // Verify that the calling app is installed itself (cf. `Add`).
  const AppId* calling_app_id = GetAppId(render_frame_host());
  if (!calling_app_id) {
    std::vector<SubAppsServiceRemoveResultPtr> result;
    for (const std::string& manifest_id_path : manifest_id_paths) {
      result.emplace_back(SubAppsServiceRemoveResult::New(
          manifest_id_path, SubAppsServiceResultCode::kFailure));
    }

    return std::move(result_callback).Run(std::move(result));
  }

  auto remove_barrier_callback =
      base::BarrierCallback<SubAppsServiceRemoveResultPtr>(
          manifest_id_paths.size(), std::move(result_callback));

  for (const std::string& manifest_id_path : manifest_id_paths) {
    RemoveSubApp(manifest_id_path, remove_barrier_callback, calling_app_id);
  }
}

void SubAppsServiceImpl::RemoveSubApp(
    const std::string& manifest_id_path,
    base::OnceCallback<void(SubAppsServiceRemoveResultPtr)> callback,
    const AppId* calling_app_id) {
  // Convert `manifest_id_path` from path form to full URL form.
  base::expected<GURL, std::string> manifest_id_with_error = ConvertPathToUrl(
      manifest_id_path, render_frame_host().GetLastCommittedOrigin());
  if (!manifest_id_with_error.has_value()) {
    // Compromised renderer, bail immediately (this call deletes *this).
    return ReportBadMessageAndDeleteThis(manifest_id_with_error.error());
  }

  const ManifestId manifest_id = GURL(manifest_id_with_error.value());
  AppId sub_app_id = GenerateAppIdFromManifestId(manifest_id);
  WebAppProvider* provider = GetWebAppProvider(render_frame_host());
  const WebApp* app = provider->registrar_unsafe().GetAppById(sub_app_id);

  // Verify that the app we're trying to remove exists, is installed and that
  // its parent_app is the one doing the current call.
  if (!app || !app->parent_app_id() ||
      *calling_app_id != *app->parent_app_id() ||
      !provider->registrar_unsafe().IsInstalled(sub_app_id)) {
    return std::move(callback).Run(SubAppsServiceRemoveResult::New(
        manifest_id_path, SubAppsServiceResultCode::kFailure));
  }

  provider->install_finalizer().UninstallExternalWebApp(
      sub_app_id, WebAppManagement::Type::kSubApp,
      webapps::WebappUninstallSource::kSubApp,
      base::BindOnce(
          [](std::string manifest_id_path,
             webapps::UninstallResultCode result_code) {
            SubAppsServiceResultCode result =
                result_code == webapps::UninstallResultCode::kSuccess
                    ? SubAppsServiceResultCode::kSuccess
                    : SubAppsServiceResultCode::kFailure;
            return SubAppsServiceRemoveResult::New(manifest_id_path, result);
          },
          manifest_id_path)
          .Then(std::move(callback)));
}

}  // namespace web_app
