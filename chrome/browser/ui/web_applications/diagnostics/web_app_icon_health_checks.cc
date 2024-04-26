// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/diagnostics/web_app_icon_health_checks.h"

#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "chrome/browser/web_applications/app_service/web_app_publisher_helper.h"
#include "chrome/browser/web_applications/commands/web_app_icon_diagnostic_command.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"

namespace web_app {

WebAppIconHealthChecks::WebAppIconHealthChecks(Profile* profile)
    : profile_(profile),
      app_type_(WebAppPublisherHelper::GetWebAppType()),
      web_apps_published_event_(profile, app_type_) {}

WebAppIconHealthChecks::~WebAppIconHealthChecks() = default;

void WebAppIconHealthChecks::Start(base::OnceClosure done_callback) {
  DCHECK(!done_callback_);
  done_callback_ = std::move(done_callback);
  web_apps_published_event_.Post(
      base::BindOnce(&WebAppIconHealthChecks::RunDiagnostics, GetWeakPtr()));
}

base::WeakPtr<WebAppIconHealthChecks> WebAppIconHealthChecks::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void WebAppIconHealthChecks::OnWebAppWillBeUninstalled(
    const webapps::AppId& app_id) {
  if (apps_running_icon_diagnostics_.erase(app_id) > 0) {
    run_complete_callback_.Run();
  }
}

void WebAppIconHealthChecks::OnWebAppInstallManagerDestroyed() {}

void WebAppIconHealthChecks::RunDiagnostics() {
  WebAppProvider* provider =
      WebAppProvider::GetForLocalAppsUnchecked(profile_.get());

  install_manager_observation_.Observe(&provider->install_manager());

  std::vector<webapps::AppId> app_ids =
      provider->registrar_unsafe().GetAppIds();
  run_complete_callback_ = base::BarrierClosure(
      app_ids.size(),
      base::BindOnce(&WebAppIconHealthChecks::RecordDiagnosticResults,
                     GetWeakPtr()));

  for (const webapps::AppId& app_id : app_ids) {
    apps_running_icon_diagnostics_.emplace(app_id);
    provider->scheduler().RunIconDiagnosticsForApp(
        app_id, base::BindOnce(&WebAppIconHealthChecks::SaveDiagnosticForApp,
                               GetWeakPtr(), app_id));
  }
}

void WebAppIconHealthChecks::SaveDiagnosticForApp(
    webapps::AppId app_id,
    std::optional<WebAppIconDiagnosticResult> result) {
  apps_running_icon_diagnostics_.erase(app_id);
  if (result)
    results_.push_back(*std::move(result));
  run_complete_callback_.Run();
}

void WebAppIconHealthChecks::RecordDiagnosticResults() {
  install_manager_observation_.Reset();

  using Result = WebAppIconDiagnosticResult;
  auto count = [&](auto member) {
    return base::ranges::count(results_, true, member);
  };

  base::UmaHistogramCounts100("WebApp.Icon.AppsWithEmptyDownloadedIconSizes",
                              count(&Result::has_empty_downloaded_icon_sizes));
  base::UmaHistogramCounts100("WebApp.Icon.AppsWithGeneratedIconFlag",
                              count(&Result::has_generated_icon_flag));
  base::UmaHistogramCounts100("WebApp.Icon.AppsWithGeneratedIconBitmap",
                              count(&Result::has_generated_icon_bitmap));
  base::UmaHistogramCounts100(
      "WebApp.Icon.AppsWithGeneratedIconFlagFalseNegative",
      count(&Result::has_generated_icon_flag_false_negative));
  base::UmaHistogramCounts100("WebApp.Icon.AppsWithEmptyIconBitmap",
                              count(&Result::has_empty_icon_bitmap));
  base::UmaHistogramCounts100("WebApp.Icon.AppsWithEmptyIconFile",
                              count(&Result::has_empty_icon_file));
  base::UmaHistogramCounts100("WebApp.Icon.AppsWithMissingIconFile",
                              count(&Result::has_missing_icon_file));
  // TODO(crbug.com/40858602):
  // Measure:
  // - Bitmap:
  //   - WebApp.Icon.AppsWithFallbackGreyBox
  //   - WebApp.Icon.AppsWithEmptyBitmapFile
  // - Aggregate:
  //   - WebApp.Icon.AppsWithBrokenState
  //   - WebApp.Icon.AppsWithBrokenStateForInstallSource
  //   - WebApp.Icon.AppsWithBrokenStateOnShelf

  std::move(done_callback_).Run();
}

}  // namespace web_app
