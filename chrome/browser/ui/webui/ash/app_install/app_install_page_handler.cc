// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/app_install/app_install_page_handler.h"

#include <utility>

#include "base/metrics/user_metrics.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/metrics/structured/event_logging_features.h"
#include "chrome/browser/ui/webui/ash/app_install/app_install.mojom.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "components/metrics/structured/structured_events.h"
#include "components/metrics/structured/structured_metrics_client.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"

namespace ash::app_install {

namespace {

namespace cros_events = metrics::structured::events::v2::cr_os_events;

int64_t ToLong(web_app::WebAppInstallStatus web_app_install_status) {
  return static_cast<int64_t>(web_app_install_status);
}

}  // namespace

AppInstallPageHandler::AppInstallPageHandler(
    Profile* profile,
    mojom::DialogArgsPtr args,
    std::string expected_app_id,
    base::OnceCallback<void(bool accepted)> dialog_accepted_callback,
    mojo::PendingReceiver<mojom::PageHandler> pending_page_handler,
    CloseDialogCallback close_dialog_callback)
    : profile_{profile},
      dialog_args_{std::move(args)},
      expected_app_id_(expected_app_id),
      dialog_accepted_callback_{std::move(dialog_accepted_callback)},
      receiver_{this, std::move(pending_page_handler)},
      close_dialog_callback_{std::move(close_dialog_callback)} {
  base::RecordAction(
      base::UserMetricsAction("ChromeOS.AppInstallDialog.Shown"));
}

AppInstallPageHandler::~AppInstallPageHandler() = default;

void AppInstallPageHandler::GetDialogArgs(GetDialogArgsCallback callback) {
  std::move(callback).Run(dialog_args_ ? dialog_args_.Clone()
                                       : mojom::DialogArgs::New());
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
                        .SetAppId(expected_app_id_)));
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
                      .SetAppId(expected_app_id_)));
  }

  install_app_callback_ = std::move(callback);
  std::move(dialog_accepted_callback_).Run(true);
}

void AppInstallPageHandler::OnInstallComplete(const std::string* app_id) {
  if (app_id) {
    app_id_ = *app_id;
    // OnInstallComplete must not be called with an 'app_id' if the expected app
    // was not able to be installed. The app_id must match also the expected app
    // id.
    CHECK_EQ(*app_id, expected_app_id_);
  }
  if (install_app_callback_) {
    std::move(install_app_callback_).Run(/*success=*/app_id);
  }
}

void AppInstallPageHandler::LaunchApp() {
  if (app_id_.empty()) {
    mojo::ReportBadMessage("Unable to launch app without an app_id.");
    return;
  }
  base::RecordAction(
      base::UserMetricsAction("ChromeOS.AppInstallDialog.AppLaunched"));
  apps::AppServiceProxyFactory::GetForProfile(profile_)->Launch(
      app_id_, ui::EF_NONE, apps::LaunchSource::kFromInstaller);
}

}  // namespace ash::app_install
