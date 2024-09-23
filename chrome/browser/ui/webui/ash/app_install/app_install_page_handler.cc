// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/app_install/app_install_page_handler.h"

#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/functional/overloaded.h"
#include "base/metrics/user_metrics.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/metrics/app_discovery_metrics.h"
#include "chrome/browser/apps/app_service/package_id_util.h"
#include "chrome/browser/ui/webui/ash/app_install/app_install.mojom.h"
#include "chrome/browser/web_applications/web_app_constants.h"
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
    std::optional<AppInstallDialogArgs> dialog_args,
    CloseDialogCallback close_dialog_callback,
    mojo::PendingReceiver<mojom::PageHandler> pending_page_handler)
    : profile_{profile},
      dialog_args_{std::move(dialog_args)},
      close_dialog_callback_{std::move(close_dialog_callback)},
      page_handler_receiver_{this, std::move(pending_page_handler)},
      app_info_actions_receiver_{this},
      connection_error_actions_receiver_{this} {
  base::RecordAction(
      base::UserMetricsAction("ChromeOS.AppInstallDialog.Shown"));

  if (GetAutoAcceptForTesting()) {
    InstallApp(base::DoNothing());
    CloseDialog();
  }
}

AppInstallPageHandler::~AppInstallPageHandler() = default;

void AppInstallPageHandler::GetDialogArgs(GetDialogArgsCallback callback) {
  if (!dialog_args_.has_value()) {
    pending_dialog_args_callbacks_.push_back(std::move(callback));
    return;
  }

  std::move(callback).Run(ConvertDialogArgsToMojom(dialog_args_.value()));
}

void AppInstallPageHandler::CloseDialog() {
  if (!dialog_args_.has_value()) {
    return;
  }

  // Ensure that `close_dialog_callback_` always runs. `close_dialog_callback_`
  // could be null if the close button is clicked multiple times . In this case,
  // the ScopedClosureRunner will not run anything.
  base::ScopedClosureRunner close_callback_runner(
      std::move(close_dialog_callback_));

  if (auto* app_info_args = absl::get_if<AppInfoArgs>(&dialog_args_.value())) {
    if (!app_info_args->dialog_accepted_callback) {
      return;
    }

    base::RecordAction(
        base::UserMetricsAction("ChromeOS.AppInstallDialog.Cancelled"));

    if (std::optional<std::string> metrics_id =
            apps::AppDiscoveryMetrics::GetAppStringToRecordForPackage(
                app_info_args->package_id)) {
      metrics::structured::StructuredMetricsClient::Record(
          cros_events::AppDiscovery_Browser_AppInstallDialogResult()
              .SetWebAppInstallStatus(
                  ToLong(web_app::WebAppInstallStatus::kCancelled))
              .SetAppId(*metrics_id));
    }

    std::move(app_info_args->dialog_accepted_callback).Run(false);
  }
}

void AppInstallPageHandler::InstallApp(InstallAppCallback callback) {
  if (!dialog_args_.has_value()) {
    return;
  }

  AppInfoArgs& app_info_args = absl::get<AppInfoArgs>(dialog_args_.value());

  base::RecordAction(
      base::UserMetricsAction("ChromeOS.AppInstallDialog.Installed"));

  if (std::optional<std::string> metrics_id =
          apps::AppDiscoveryMetrics::GetAppStringToRecordForPackage(
              app_info_args.package_id)) {
    metrics::structured::StructuredMetricsClient::Record(
        cros_events::AppDiscovery_Browser_AppInstallDialogResult()
            .SetWebAppInstallStatus(
                ToLong(web_app::WebAppInstallStatus::kAccepted))
            .SetAppId(*metrics_id));
  }

  install_app_callback_ = std::move(callback);
  std::move(app_info_args.dialog_accepted_callback).Run(true);
}

void AppInstallPageHandler::SetDialogArgs(AppInstallDialogArgs dialog_args) {
  CHECK(!dialog_args_.has_value());
  dialog_args_ = std::move(dialog_args);
  for (GetDialogArgsCallback& callback : pending_dialog_args_callbacks_) {
    std::move(callback).Run(ConvertDialogArgsToMojom(dialog_args_.value()));
  }
  pending_dialog_args_callbacks_.clear();
}

void AppInstallPageHandler::OnInstallComplete(
    bool success,
    std::optional<base::OnceCallback<void(bool accepted)>> retry_callback) {
  if (!dialog_args_.has_value()) {
    return;
  }

  if (!success) {
    CHECK(retry_callback.has_value());
    absl::get<AppInfoArgs>(dialog_args_.value()).dialog_accepted_callback =
        std::move(retry_callback.value());
  }
  if (install_app_callback_) {
    std::move(install_app_callback_).Run(success);
  }
}

void AppInstallPageHandler::LaunchApp() {
  if (!dialog_args_.has_value()) {
    return;
  }

  std::optional<std::string> app_id = apps_util::GetAppWithPackageId(
      &*profile_, absl::get<AppInfoArgs>(dialog_args_.value()).package_id);
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
  if (!dialog_args_.has_value()) {
    return;
  }

  base::OnceClosure& callback =
      absl::get<ConnectionErrorArgs>(dialog_args_.value()).try_again_callback;
  if (callback) {
    std::move(callback).Run();
  }
}

mojom::DialogArgsPtr AppInstallPageHandler::ConvertDialogArgsToMojom(
    const AppInstallDialogArgs& dialog_args) {
  return absl::visit(
      base::Overloaded(
          [&](const AppInfoArgs& app_info_args) {
            return mojom::DialogArgs::NewAppInfoArgs(mojom::AppInfoArgs::New(
                app_info_args.data.Clone(),
                app_info_actions_receiver_.BindNewPipeAndPassRemote()));
          },
          [&](const NoAppErrorArgs& no_app_error_args) {
            return mojom::DialogArgs::NewNoAppErrorArgs(
                mojom::NoAppErrorArgs::New());
          },
          [&](const ConnectionErrorArgs& connection_error_args) {
            return mojom::DialogArgs::NewConnectionErrorActions(
                connection_error_actions_receiver_.BindNewPipeAndPassRemote());
          }),
      dialog_args);
}

}  // namespace ash::app_install
