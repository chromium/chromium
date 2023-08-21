// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_PREINSTALLED_WEB_APP_WINDOW_EXPERIMENT_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_PREINSTALLED_WEB_APP_WINDOW_EXPERIMENT_H_

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/one_shot_event.h"
#include "base/scoped_observation.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom-forward.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registrar_observer.h"
#include "components/services/app_service/public/cpp/preferred_apps_list_handle.h"

class Profile;

namespace web_app {

// Controls whether the cleanup and experiment-state-persisting code runs when
// the experiment ends (when `kPreinstalledWebAppWindowExperiment` is disabled).
BASE_DECLARE_FEATURE(kWebAppWindowExperimentCleanup);

// Sets up and manages a CrOS-only experiment for opening preinstalled web apps
// in windows with link capturing.
// - Owned/started by the PreinstalledWebAppManager, and may set in-memory
//   overrides for user display mode on the WebAppRegistrar.
// - Persists values to prefs for experiment eligibility and user group, apps
//   launched before the experiment, and apps with user-set display mode. See
//   preinstalled_web_app_window_experiment_utils.
// - Experiment results are emitted by tagging existing web app DailyInteraction
//   metrics with experiment state (ie. user group etc. persisted to  prefs) and
//   by emitting when the user changes display mode or link capturing.
// - See https://app.code2flow.com/SLbPlsrL5VjH for a flow chart of the process.
// - Can be deleted once the experiment has run (likely by late 2023). See
//   http://crbug.com/1385246 for details.
class PreinstalledWebAppWindowExperiment
    : public WebAppRegistrarObserver,
      public apps::PreferredAppsListHandle::Observer {
 public:
  explicit PreinstalledWebAppWindowExperiment(Profile* profile);
  PreinstalledWebAppWindowExperiment(
      const PreinstalledWebAppWindowExperiment&) = delete;
  PreinstalledWebAppWindowExperiment& operator=(
      const PreinstalledWebAppWindowExperiment&) = delete;
  ~PreinstalledWebAppWindowExperiment() override;

  // Sets up initial state of experiment, if needed, and starts observing for
  // changes to relevant state.
  void Start();
  // Notifies this class that preinstalled apps are now set up.
  void NotifyPreinstalledAppsInstalled();

  // Signals when the class is done setting up, or when it has determined no
  // setup is needed.
  const base::OneShotEvent& setup_done_for_testing() {
    return setup_done_for_testing_;
  }

  // Signals when preinstalled apps have been installed by the
  // PreinstalledWebAppManager. Exposed for testing.
  const base::OneShotEvent& preinstalled_apps_installed_for_testing() {
    return preinstalled_apps_installed_;
  }

 private:
  void CheckEligible();
  void FirstTimeSetup();
  void SetFirstTimePrefsThenMaybeStart();
  void StartOverridesAndObservations();
  void CleanUp();

  // WebAppRegistrarObserver:
  void OnAppRegistrarDestroyed() override;
  void OnWebAppUserDisplayModeChanged(
      const AppId& app_id,
      mojom::UserDisplayMode user_display_mode) override;

  // PreferredAppsListHandle::Observer:
  void OnPreferredAppChanged(const std::string& app_id,
                             bool is_preferred_app) override;
  void OnPreferredAppsListWillBeDestroyed(
      apps::PreferredAppsListHandle* handle) override;

  WebAppRegistrar& registrar_unsafe() const;

  // Set of apps for which the experiment called `SetSupportedLinksPreference`
  // and hasn't yet observed a resulting `OnPreferredAppChanged`.
  base::flat_set<AppId> apps_that_experiment_setup_set_supported_links_;

  const raw_ptr<Profile> profile_;
  base::OneShotEvent preinstalled_apps_installed_;

  base::OneShotEvent setup_done_for_testing_;

  base::ScopedObservation<WebAppRegistrar, WebAppRegistrarObserver>
      registrar_observation_{this};

  base::ScopedObservation<apps::PreferredAppsListHandle,
                          apps::PreferredAppsListHandle::Observer>
      preferred_apps_observation_{this};

  base::WeakPtrFactory<PreinstalledWebAppWindowExperiment> weak_ptr_factory_{
      this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_PREINSTALLED_WEB_APP_WINDOW_EXPERIMENT_H_
