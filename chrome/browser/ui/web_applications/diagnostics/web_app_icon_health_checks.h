// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_DIAGNOSTICS_WEB_APP_ICON_HEALTH_CHECKS_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_DIAGNOSTICS_WEB_APP_ICON_HEALTH_CHECKS_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/web_applications/diagnostics/app_type_initialized_event.h"
#include "chrome/browser/ui/web_applications/diagnostics/web_app_icon_diagnostic.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"

class Profile;

namespace web_app {

// Runs a suite of icon diagnostics on all installed web app icons at start up
// (triggered by WebAppMetrics) and records aggregate metrics for any broken
// states detected.
class WebAppIconHealthChecks : public WebAppInstallManagerObserver {
 public:
  explicit WebAppIconHealthChecks(Profile* profile);
  ~WebAppIconHealthChecks() override;

  void Start(base::OnceClosure done_callback);

  base::WeakPtr<WebAppIconHealthChecks> GetWeakPtr();

  // WebAppInstallManagerObserver:
  void OnWebAppWillBeUninstalled(const AppId& app_id) override;
  void OnWebAppInstallManagerDestroyed() override;

 private:
  void RunDiagnostics();
  void SaveDiagnosticForApp(
      AppId app_id,
      absl::optional<WebAppIconDiagnostic::Result> result);
  void RecordDiagnosticResults();

  raw_ptr<Profile> profile_;
  apps::AppType app_type_;
  AppTypeInitializedEvent web_apps_published_event_;

  base::flat_map<AppId, std::unique_ptr<WebAppIconDiagnostic>>
      running_diagnostics_;
  base::RepeatingClosure run_complete_callback_;
  std::vector<WebAppIconDiagnostic::Result> results_;
  base::OnceClosure done_callback_;

  base::ScopedObservation<WebAppInstallManager, WebAppInstallManagerObserver>
      install_manager_observation_{this};

  base::WeakPtrFactory<WebAppIconHealthChecks> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_DIAGNOSTICS_WEB_APP_ICON_HEALTH_CHECKS_H_
