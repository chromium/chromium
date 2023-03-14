// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/sub_apps_service_impl.h"

#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/types/expected.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/commands/sub_app_install_command.h"
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
using blink::mojom::SubAppsServiceResultCode;

namespace web_app {

namespace {

// Resolve string `path` with `origin`, and if the resulting GURL isn't same
// origin with `origin` then return an error (for which the caller needs to
// raise a `ReportBadMessageAndDeleteThis`).
base::expected<std::string, std::string> ConvertPathToUrl(
    const std::string& path,
    const url::Origin& origin) {
  GURL resolved = origin.GetURL().Resolve(path);

  if (!origin.IsSameOriginWith(resolved)) {
    return base::unexpected(
        "SubAppsServiceImpl: Different origin arg to that of the calling app.");
  }

  return base::ok(resolved.spec());
}

std::string ConvertUrlToPath(const UnhashedAppId& unhashed_app_id) {
  return GURL(unhashed_app_id).PathForRequest();
}

base::expected<std::vector<std::pair<UnhashedAppId, GURL>>, std::string>
AddOptionsFromMojo(
    const url::Origin& origin,
    const std::vector<SubAppsServiceAddParametersPtr>& sub_apps_to_add_mojo) {
  std::vector<std::pair<UnhashedAppId, GURL>> sub_apps;
  for (const auto& sub_app : sub_apps_to_add_mojo) {
    base::expected<std::string, std::string> unhashed_app_id =
        ConvertPathToUrl(sub_app->unhashed_app_id_path, origin);
    if (!unhashed_app_id.has_value()) {
      return base::unexpected(unhashed_app_id.error());
    }
    base::expected<std::string, std::string> install_url =
        ConvertPathToUrl(sub_app->install_url_path, origin);
    if (!install_url.has_value()) {
      return base::unexpected(install_url.error());
    }
    sub_apps.emplace_back(unhashed_app_id.value(), install_url.value());
  }
  return sub_apps;
}

SubAppsServiceImpl::AddResultsMojo AddResultsToMojo(
    const SubAppsServiceImpl::AddResults& add_results) {
  SubAppsServiceImpl::AddResultsMojo add_results_mojo;
  for (const auto& [unhashed_app_id, result_code] : add_results) {
    add_results_mojo.emplace_back(SubAppsServiceAddResult::New(
        ConvertUrlToPath(unhashed_app_id), result_code));
  }
  return add_results_mojo;
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

void OnAdd(SubAppsServiceImpl::AddCallback result_callback,
           SubAppsServiceImpl::AddResults results) {
  std::move(result_callback).Run(AddResultsToMojo(results));
}

void OnRemove(SubAppsServiceImpl::RemoveCallback result_callback,
              webapps::UninstallResultCode code) {
  std::move(result_callback)
      .Run(code == webapps::UninstallResultCode::kSuccess
               ? SubAppsServiceResultCode::kSuccess
               : SubAppsServiceResultCode::kFailure);
}

}  // namespace

SubAppsServiceImpl::SubAppsServiceImpl(
    content::RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<SubAppsService> receiver)
    : DocumentService(render_frame_host, std::move(receiver)) {}

SubAppsServiceImpl::~SubAppsServiceImpl() = default;

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
  // Verify that the calling app is installed itself. This check is done here
  // and not in `CreateIfAllowed` because of a potential race between doing the
  // check there and then running the current function, and the parent app being
  // installed/uninstalled.
  if (!parent_app_id) {
    std::vector<SubAppsServiceAddResultPtr> result;
    for (const auto& sub_app : sub_apps_to_add) {
      result.emplace_back(SubAppsServiceAddResult::New(
          sub_app->unhashed_app_id_path, SubAppsServiceResultCode::kFailure));
    }
    return std::move(result_callback).Run(/*mojom_results=*/std::move(result));
  }

  base::expected<std::vector<std::pair<UnhashedAppId, GURL>>, std::string>
      add_options = AddOptionsFromMojo(
          render_frame_host().GetLastCommittedOrigin(), sub_apps_to_add);
  if (!add_options.has_value()) {
    // Compromised renderer, bail immediately (this call deletes *this).
    return ReportBadMessageAndDeleteThis(add_options.error());
  }

  auto install_command = std::make_unique<SubAppInstallCommand>(
      *parent_app_id, add_options.value(),
      base::BindOnce(&OnAdd, std::move(result_callback)),
      Profile::FromBrowserContext(render_frame_host().GetBrowserContext()),
      std::make_unique<WebAppUrlLoader>(),
      std::make_unique<WebAppDataRetriever>());

  provider->command_manager().ScheduleCommand(std::move(install_command));
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
    UnhashedAppId unhashed_app_id =
        GenerateAppIdUnhashed(sub_app->manifest_id(), sub_app->start_url());
    sub_apps_list.push_back(SubAppsServiceListResultEntry::New(
        ConvertUrlToPath(unhashed_app_id), sub_app->untranslated_name()));
  }

  std::move(result_callback)
      .Run(SubAppsServiceListResult::New(SubAppsServiceResultCode::kSuccess,
                                         std::move(sub_apps_list)));
}

void SubAppsServiceImpl::Remove(const UnhashedAppId& unhashed_app_id_path,
                                RemoveCallback result_callback) {
  WebAppProvider* provider = GetWebAppProvider(render_frame_host());
  if (!provider->on_registry_ready().is_signaled()) {
    provider->on_registry_ready().Post(
        FROM_HERE,
        base::BindOnce(&SubAppsServiceImpl::Remove,
                       weak_ptr_factory_.GetWeakPtr(), unhashed_app_id_path,
                       std::move(result_callback)));
    return;
  }

  // Verify that the calling app is installed itself (cf. `Add`).
  const AppId* calling_app_id = GetAppId(render_frame_host());
  if (!calling_app_id) {
    return std::move(result_callback).Run(SubAppsServiceResultCode::kFailure);
  }

  // Convert `unhashed_app_id_path` from path form to full URL form.
  base::expected<std::string, std::string> unhashed_app_id = ConvertPathToUrl(
      unhashed_app_id_path, render_frame_host().GetLastCommittedOrigin());
  if (!unhashed_app_id.has_value()) {
    // Compromised renderer, bail immediately (this call deletes *this).
    return ReportBadMessageAndDeleteThis(unhashed_app_id.error());
  }

  AppId sub_app_id = GenerateAppIdFromUnhashed(unhashed_app_id.value());
  const WebApp* app = provider->registrar_unsafe().GetAppById(sub_app_id);

  // Verify that the app we're trying to remove exists, that its parent_app is
  // the one doing the current call, and that the app was locally installed.
  if (!app || !app->parent_app_id() ||
      *calling_app_id != *app->parent_app_id() ||
      !app->is_locally_installed()) {
    return std::move(result_callback).Run(SubAppsServiceResultCode::kFailure);
  }

  provider->install_finalizer().UninstallExternalWebApp(
      sub_app_id, WebAppManagement::Type::kSubApp,
      webapps::WebappUninstallSource::kSubApp,
      base::BindOnce(&OnRemove, std::move(result_callback)));
}

}  // namespace web_app
