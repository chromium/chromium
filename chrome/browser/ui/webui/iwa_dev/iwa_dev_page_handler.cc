// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/iwa_dev/iwa_dev_page_handler.h"

#include <optional>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/files/file_path.h"
#include "base/notreached.h"
#include "base/types/expected_macros.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolated_web_apps/install/isolated_web_app_dev_install_manager.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_features.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/model/isolation_data.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "components/webapps/isolated_web_apps/types/iwa_origin.h"
#include "components/webapps/isolated_web_apps/types/source.h"
#include "components/webapps/isolated_web_apps/types/storage_location.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "url/gurl.h"

namespace {

iwa_dev::mojom::IwaDevModeSourcePtr MapToMojomIwaDevModeSource(
    const web_app::IsolationData& isolation_data) {
  if (isolation_data.update_manifest_url()) {
    return iwa_dev::mojom::IwaDevModeSource::NewUpdateInfo(
        iwa_dev::mojom::UpdateInfo::New(
            *isolation_data.update_manifest_url(),
            isolation_data.update_channel()
                .value_or(web_app::UpdateChannel::default_channel())
                .ToString()));
  }

  return std::visit(
      absl::Overload{
          [](const web_app::IwaStorageOwnedBundle& location)
              -> iwa_dev::mojom::IwaDevModeSourcePtr {
            return iwa_dev::mojom::IwaDevModeSource::NewBundlePath(
                base::FilePath(FILE_PATH_LITERAL("..."))
                    .AppendASCII(location.dir_name_ascii())
                    .Append(web_app::kMainSwbnFileName));
          },
          [](const web_app::IwaStorageUnownedBundle& location)
              -> iwa_dev::mojom::IwaDevModeSourcePtr {
            return iwa_dev::mojom::IwaDevModeSource::NewBundlePath(
                base::FilePath(FILE_PATH_LITERAL("..."))
                    .Append(location.path().DirName().BaseName())
                    .Append(location.path().BaseName()));
          },
          [](const web_app::IwaStorageProxy& location)
              -> iwa_dev::mojom::IwaDevModeSourcePtr {
            return iwa_dev::mojom::IwaDevModeSource::NewProxyOrigin(
                location.proxy_url());
          },
      },
      isolation_data.location().variant());
}

iwa_dev::mojom::IwaDevModeAppInfoPtr MapToMojomIwaDevModeAppInfo(
    const web_app::WebApp& app) {
  auto iwa_origin = web_app::IwaOrigin::Create(app.start_url()).value();
  std::string web_bundle_id = iwa_origin.web_bundle_id().id();

  const web_app::IsolationData& isolation_data = app.isolation_data().value();

  return iwa_dev::mojom::IwaDevModeAppInfo::New(
      app.app_id(), std::move(web_bundle_id), app.untranslated_name(),
      MapToMojomIwaDevModeSource(isolation_data),
      isolation_data.version().GetString());
}

std::optional<std::string> MapToInstallError(
    web_app::IsolatedWebAppDevInstallManager::
        MaybeInstallIsolatedWebAppCommandSuccess result) {
  return result.has_value() ? std::nullopt : std::make_optional(result.error());
}

}  // namespace

IwaDevPageHandler::IwaDevPageHandler(
    content::WebUI* web_ui,
    mojo::PendingRemote<iwa_dev::mojom::Page> page,
    mojo::PendingReceiver<iwa_dev::mojom::PageHandler> receiver)
    : profile_(CHECK_DEREF(Profile::FromWebUI(web_ui))),
      provider_(
          CHECK_DEREF(web_app::WebAppProvider::GetForWebApps(&profile_.get()))),
      web_contents_(CHECK_DEREF(web_ui->GetWebContents())),
      receiver_(this, std::move(receiver)),
      page_(std::move(page)) {
  install_observation_.Observe(&provider_->install_manager());
}

IwaDevPageHandler::~IwaDevPageHandler() = default;

void IwaDevPageHandler::GetInstalledAppsInfo(
    GetInstalledAppsInfoCallback callback) {
  std::vector<iwa_dev::mojom::IwaDevModeAppInfoPtr> dev_mode_apps;
  for (const web_app::WebApp& app : provider_->registrar_unsafe().GetApps(
           web_app::WebAppFilter::IsDevModeIsolatedApp())) {
    dev_mode_apps.push_back(MapToMojomIwaDevModeAppInfo(app));
  }

  std::move(callback).Run(std::move(dev_mode_apps));
}

void IwaDevPageHandler::UninstallApp(const std::string& app_id,
                                     UninstallAppCallback callback) {
  // If the app is no longer installed (e.g. uninstalled via another window),
  // or the webui compromised, this should be ignored.
  if (!provider_->registrar_unsafe().GetAppById(
          app_id, web_app::WebAppFilter::IsDevModeIsolatedApp())) {
    std::move(callback).Run(false);
    return;
  }

  provider_->ui_manager().PresentUserUninstallDialog(
      app_id, webapps::WebappUninstallSource::kAppsPage,
      web_contents_->GetTopLevelNativeWindow(),
      base::BindOnce(&webapps::UninstallSucceeded).Then(std::move(callback)));
}

void IwaDevPageHandler::InstallAppFromDevProxy(
    const GURL& url,
    InstallAppFromDevProxyCallback callback) {
  provider_->isolated_web_app_dev_install_manager()
      .InstallIsolatedWebAppFromDevModeProxy(
          url, web_app::IsolatedWebAppDevInstallManager::InstallSurface::kDevUi,
          base::BindOnce(&MapToInstallError).Then(std::move(callback)));
}

void IwaDevPageHandler::OnWebAppInstalled(const webapps::AppId& app_id) {
  if (const web_app::WebApp* app = provider_->registrar_unsafe().GetAppById(
          app_id, web_app::WebAppFilter::IsDevModeIsolatedApp())) {
    page_->OnAppInstalled(MapToMojomIwaDevModeAppInfo(*app));
  }
}

void IwaDevPageHandler::OnWebAppManifestUpdated(const webapps::AppId& app_id) {
  if (const web_app::WebApp* app = provider_->registrar_unsafe().GetAppById(
          app_id, web_app::WebAppFilter::IsDevModeIsolatedApp())) {
    page_->OnAppUpdated(MapToMojomIwaDevModeAppInfo(*app));
  }
}

void IwaDevPageHandler::OnWebAppWillBeUninstalled(
    const webapps::AppId& app_id) {
  if (provider_->registrar_unsafe().AppMatches(
          app_id, web_app::WebAppFilter::IsDevModeIsolatedApp())) {
    page_->OnAppUninstalled(app_id);
  }
}

void IwaDevPageHandler::OnWebAppInstallManagerDestroyed() {
  install_observation_.Reset();
}
