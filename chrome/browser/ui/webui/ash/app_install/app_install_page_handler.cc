// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/app_install/app_install_page_handler.h"

#include <utility>

#include "base/metrics/user_metrics.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/package_id_util.h"
#include "chrome/browser/metrics/structured/event_logging_features.h"
#include "chrome/browser/ui/webui/ash/app_install/app_install.mojom.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "components/metrics/structured/structured_events.h"
#include "components/metrics/structured/structured_metrics_client.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_types.h"

namespace ash::app_install {

namespace {

namespace cros_events = metrics::structured::events::v2::cr_os_events;

int64_t ToLong(web_app::WebAppInstallStatus web_app_install_status) {
  return static_cast<int64_t>(web_app_install_status);
}

bool g_auto_accept_for_testing = false;

// TODO(b/330414871): AppInstallService shouldn't know about publisher specific
// logic, remove the generation of app_ids.
std::string GetAppId(const apps::PackageId& package_id) {
  CHECK_EQ(package_id.package_type(), apps::PackageType::kWeb);
  // data->package_id.identifier() is the manifest ID for web apps.
  return web_app::GenerateAppIdFromManifestId(GURL(package_id.identifier()));
}

}  // namespace

// static
bool AppInstallPageHandler::GetAutoAcceptForTesting() {
  return g_auto_accept_for_testing;
}

// static
void AppInstallPageHandler::SetAutoAcceptForTesting(bool auto_accept) {
  g_auto_accept_for_testing = auto_accept;
}

AppInstallPageHandler::AppInstallPageHandler(
    Profile* profile,
    mojom::DialogArgsPtr args,
    apps::PackageId package_id,
    base::OnceCallback<void(bool accepted)> dialog_accepted_callback,
    CloseDialogCallback close_dialog_callback,
    base::OnceClosure try_again_callback,
    mojo::PendingReceiver<mojom::PageHandler> pending_page_handler)
    : profile_{profile},
      dialog_args_{std::move(args)},
      package_id_{std::move(package_id)},
      dialog_accepted_callback_{std::move(dialog_accepted_callback)},
      close_dialog_callback_{std::move(close_dialog_callback)},
      try_again_callback_{std::move(try_again_callback)},
      receiver_{this, std::move(pending_page_handler)} {
  base::RecordAction(
      base::UserMetricsAction("ChromeOS.AppInstallDialog.Shown"));

  if (GetAutoAcceptForTesting()) {
    InstallApp(base::DoNothing());
    CloseDialog();
  }
}

AppInstallPageHandler::~AppInstallPageHandler() = default;

void AppInstallPageHandler::GetDialogArgs(GetDialogArgsCallback callback) {
  std::move(callback).Run(dialog_args_.Clone());
}

void AppInstallPageHandler::CloseDialog() {
  if (dialog_accepted_callback_) {
    base::RecordAction(
        base::UserMetricsAction("ChromeOS.AppInstallDialog.Cancelled"));

    if (base::FeatureList::IsEnabled(
            metrics::structured::kAppDiscoveryLogging)) {
      metrics::structured::StructuredMetricsClient::Record(
          std::move(cros_events::AppDiscovery_Browser_AppInstallDialogResult()
                        .SetWebAppInstallStatus(
                            ToLong(web_app::WebAppInstallStatus::kCancelled))
                        // TODO(b/333643533): This should be using
                        // AppDiscoveryMetrics::GetAppStringToRecord().
                        .SetAppId(GetAppId(package_id_))));
    }
    std::move(dialog_accepted_callback_).Run(false);
  }

  // The callback could be null if the close button is clicked a second time
  // before the dialog closes.
  if (close_dialog_callback_) {
    std::move(close_dialog_callback_).Run();
  }
}

void AppInstallPageHandler::InstallApp(InstallAppCallback callback) {
  base::RecordAction(
      base::UserMetricsAction("ChromeOS.AppInstallDialog.Installed"));
  if (base::FeatureList::IsEnabled(metrics::structured::kAppDiscoveryLogging)) {
    metrics::structured::StructuredMetricsClient::Record(
        std::move(cros_events::AppDiscovery_Browser_AppInstallDialogResult()
                      .SetWebAppInstallStatus(
                          ToLong(web_app::WebAppInstallStatus::kAccepted))
                      // TODO(b/333643533): This should be using
                      // AppDiscoveryMetrics::GetAppStringToRecord().
                      .SetAppId(GetAppId(package_id_))));
  }

  install_app_callback_ = std::move(callback);
  std::move(dialog_accepted_callback_).Run(true);
}

void AppInstallPageHandler::OnInstallComplete(
    bool success,
    std::optional<base::OnceCallback<void(bool accepted)>> retry_callback) {
  if (!success) {
    CHECK(retry_callback.has_value());
    dialog_accepted_callback_ = std::move(retry_callback.value());
  }
  if (install_app_callback_) {
    std::move(install_app_callback_).Run(success);
  }
}

void AppInstallPageHandler::LaunchApp() {
  std::optional<std::string> app_id =
      apps_util::GetAppWithPackageId(&*profile_, package_id_);
  if (!app_id.has_value()) {
    mojo::ReportBadMessage("Unable to launch app without an app_id.");
    return;
  }
  base::RecordAction(
      base::UserMetricsAction("ChromeOS.AppInstallDialog.AppLaunched"));
  apps::AppServiceProxyFactory::GetForProfile(profile_)->Launch(
      app_id.value(), ui::EF_NONE, apps::LaunchSource::kFromInstaller);
}

void AppInstallPageHandler::TryAgain() {
  if (try_again_callback_) {
    std::move(try_again_callback_).Run();
  }
}

}  // namespace ash::app_install
