// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_DIAGNOSTICS_WEB_APP_ICON_HEALTH_CHECKS_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_DIAGNOSTICS_WEB_APP_ICON_HEALTH_CHECKS_H_

#include "base/containers/flat_set.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/web_applications/diagnostics/app_type_initialized_event.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_manager_observer.h"
#include "components/webapps/common/web_app_id.h"

class Profile;

namespace web_app {

struct WebAppIconDiagnosticResult;

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
  void OnWebAppWillBeUninstalled(const webapps::AppId& app_id) override;
  void OnWebAppInstallManagerDestroyed() override;

 private:
  void RunDiagnostics();
  void SaveDiagnosticForApp(webapps::AppId app_id,
                            std::optional<WebAppIconDiagnosticResult> result);
  void RecordDiagnosticResults();

  raw_ptr<Profile> profile_ = nullptr;
  apps::AppType app_type_;
  AppTypeInitializedEvent web_apps_published_event_;

  base::flat_set<webapps::AppId> apps_running_icon_diagnostics_;
  base::RepeatingClosure run_complete_callback_;
  std::vector<WebAppIconDiagnosticResult> results_;
  base::OnceClosure done_callback_;

  base::ScopedObservation<WebAppInstallManager, WebAppInstallManagerObserver>
      install_manager_observation_{this};

  base::WeakPtrFactory<WebAppIconHealthChecks> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_DIAGNOSTICS_WEB_APP_ICON_HEALTH_CHECKS_H_
