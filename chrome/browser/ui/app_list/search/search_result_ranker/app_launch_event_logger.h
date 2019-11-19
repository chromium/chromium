// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_APP_LAUNCH_EVENT_LOGGER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_APP_LAUNCH_EVENT_LOGGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/values.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/app_launch_event_logger.pb.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/ml_app_rank_provider.h"
#include "extensions/browser/extension_registry.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace chromeos {
namespace power {
namespace ml {
class RecentEventsCounter;
}  // namespace ml
}  // namespace power
}  // namespace chromeos

namespace ukm {
namespace builders {
class AppListAppClickData;
}  // namespace builders
}  // namespace ukm

namespace app_list {

// This class logs metrics associated with clicking on apps in ChromeOS. It also
// uses the feature data to create app rankings. These rankings are calculated
// using inference with the aggregated ML model.
//
// Logging is restricted to Arc apps with sync enabled, Chrome apps from the
// app store, PWAs and bookmark apps. This class uses UKM for logging,
// however, the metrics are not keyed by navigational urls. Instead, for Chrome
// apps the keys are based upon the app id, for Arc apps the keys are based upon
// a hash of the package name, and for PWAs and bookmark apps the keys are the
// urls associated with the PWA/bookmark.
// At the time of app launch this class logs metrics about the app clicked on
// and up to another 25 apps that were not clicked on, chosen at random.
class AppLaunchEventLogger {
 public:
  AppLaunchEventLogger();
  virtual ~AppLaunchEventLogger();

  // Processes a click on an app in the suggestion chip or search box and logs
  // the resulting metrics in UKM. This method calls EnforceLoggingPolicy() to
  // ensure the logging policy is complied with.
  void OnSuggestionChipOrSearchBoxClicked(const std::string& id,
                                          int suggestion_index,
                                          int launched_from);
  // Processes a click on an app located in the grid of apps in the launcher and
  // logs the resulting metrics in UKM. This method calls EnforceLoggingPolicy()
  // to ensure the logging policy is complied with.
  void OnGridClicked(const std::string& id);
  // Runs the inference to rank the apps. Call RetrieveRankings() to get the
  // results. The inference is performed asynchronously and no guarantees are
  // given given as to when results will be available.
  void CreateRankings();
  // Returns a map of app ids to ranking score. This will be an empty map unless
  // CreateRankings() has been called. It will be incomplete until all of
  // CreateRankings' asynchronous calls have completed.
  std::map<std::string, float> RetrieveRankings();

  static const char kPackageName[];
  static const char kShouldSync[];

 protected:
  // Get the url used to launch a PWA or bookmark app.
  virtual const GURL& GetLaunchWebURL(const extensions::Extension* extension);
  // Enforces logging policy, ensuring that the |app_features_map_| flags the
  // apps that are allowed to be logged. All apps are rechecked in case they
  // have been uninstalled since the previous check.
  void EnforceLoggingPolicy();

  // The arc apps installed on the device.
  const base::DictionaryValue* arc_apps_ = nullptr;
  // The arc packages installed on the device.
  const base::DictionaryValue* arc_packages_ = nullptr;
  // The Chrome extension registry.
  extensions::ExtensionRegistry* registry_ = nullptr;

 private:
  // Removes any leading "chrome-extension://" or "arc://". Also remove any
  // trailing "/".
  std::string RemoveScheme(const std::string& id);

  // Set registry_ to the ExtensionRegistry of the primary user and load that
  // user's Arc++ apps and Arc++ packages. Tests will exit this method early,
  // preventing the changing of these member variables from their preset test
  // values.
  void SetRegistryAndArcInfo();
  // If the app is not in |app_features_map_| then add it.
  void AddAppIfMissing(AppLaunchEvent_AppType app_type,
                       const std::string& app_id,
                       const std::string& arc_package_name,
                       const std::string& pwa_url,
                       bool is_policy_compliant);
  // Update the click rank (which ranks app by the number of clicks) for the
  // apps that have been clicked.
  void UpdateClickRank();
  // Updates the app data following a click.
  void ProcessClick(const AppLaunchEvent& event, const base::Time& now);
  // Returns a source id. |arc_package_name| is only required for Arc apps,
  // |pwa_url| is only required for PWAs and bookmark apps.
  ukm::SourceId GetSourceId(AppLaunchEvent_AppType app_type,
                            const std::string& app_id,
                            const std::string& arc_package_name,
                            const std::string& pwa_url);
  // Chooses up to 25 random apps to log, plus the app clicked on.
  // If there are fewer than 25 apps that can be logged on the device, logs
  // every app once.
  std::vector<std::string> ChooseAppsToLog(const std::string clicked_app_id);
  // Records a UMA histogram of the app type clicked on.
  void RecordAppTypeClicked(AppLaunchEvent_AppType app_type);
  // Helper function to log the clicks each hour metrics.
  void LogClicksEachHour(
      const AppLaunchFeatures& app_launch_features,
      ukm::builders::AppListAppClickData* const app_click_data);
  // Logs the app click using UKM.
  void Log(AppLaunchEvent app_launch_event);

  // A map from app id to features. Only contains apps satisfying logging
  // policy.
  base::flat_map<std::string, AppLaunchFeatures> app_features_map_;
  // A map from app id to a counter of the number of clicks in the last hour.
  // Has a time resolution one minute.
  base::flat_map<std::string,
                 std::unique_ptr<chromeos::power::ml::RecentEventsCounter>>
      app_clicks_last_hour_;
  // A map from app id to a counter of the number of clicks in the last 24
  // hours. Has a time resolution of 15 minutes.
  base::flat_map<std::string,
                 std::unique_ptr<chromeos::power::ml::RecentEventsCounter>>
      app_clicks_last_24_hours_;

  // The time this class was instantiated. Allows duration to be calculated.
  base::Time start_time_;
  // A counter for the click in the last hour. Has a time resolution of 1
  // minute.
  const std::unique_ptr<chromeos::power::ml::RecentEventsCounter>
      all_clicks_last_hour_;
  // A counter for the clicks in the last 24 hours. Has a time resolution of 15
  // minutes.
  const std::unique_ptr<chromeos::power::ml::RecentEventsCounter>
      all_clicks_last_24_hours_;

  // Empty until/unless CreateRankings is called.
  std::unique_ptr<MlAppRankProvider> ml_app_rank_provider_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::WeakPtrFactory<AppLaunchEventLogger> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AppLaunchEventLogger);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_APP_LAUNCH_EVENT_LOGGER_H_
