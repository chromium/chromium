// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_ARC_ARC_APP_REINSTALL_SEARCH_PROVIDER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_ARC_ARC_APP_REINSTALL_SEARCH_PROVIDER_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/search/arc/arc_app_reinstall_app_result.h"
#include "chrome/browser/ui/app_list/search/search_provider.h"
#include "ui/gfx/image/image_skia.h"

class PrefRegistrySimple;
class Profile;
FORWARD_DECLARE_TEST(ArcAppReinstallSearchProviderTest,
                     TestResultsWithSearchChanged);
FORWARD_DECLARE_TEST(ArcAppReinstallSearchProviderTest,
                     TestResultsWithAppsChanged);
FORWARD_DECLARE_TEST(ArcAppReinstallSearchProviderTest,
                     TestResultListComparison);
FORWARD_DECLARE_TEST(ArcAppReinstallSearchProviderTest, TestShouldShowAnything);

namespace app_list {
// A search provider that returns app candidates that are reinstallation
// candidates. The current provider of candidates for this provider is the Fast
// App Reinstall API. This Provider returns a list of applications, in
// preference order. Apps are returned as cards for empty query items - and with
// queries we match with regular expression and then return only matching as app
// list results.
//
// For users who do not have ARC++ enabled, we do not make a call through to the
// Play Store, but rather populate with empty results.
class ArcAppReinstallSearchProvider
    : public SearchProvider,
      public ArcAppListPrefs::Observer,
      public ArcAppReinstallAppResult::Observer {
 public:
  // Fields for working with pref syncable state.
  // constants used for prefs.
  static constexpr char kInstallTime[] = "install_time";

  // Overall dictionary to use for all arc app reinstall states.
  static constexpr char kAppState[] = "arc_app_reinstall_state";

  // field name for install start time, as milliseconds since epoch
  static constexpr char kInstallStartTime[] = "install_start_time";

  // field name for install opened time, as milliseconds since epoch
  static constexpr char kOpenTime[] = "open_time";
  // field name for uninstalltime, as milliseconds since epoch.
  static constexpr char kUninstallTime[] = "uninstall_time";

  // field name for latest impressiontime, as milliseconds since epoch.
  static constexpr char kImpressionTime[] = "impression_time";
  // Number of impressions.
  static constexpr char kImpressionCount[] = "impression_count";

  // Constructor receives the Profile in order to
  // instantiate App results. Ownership is not taken.
  //
  // We also give a max result count, so that the provider only instantiates the
  // correct number of results for output.
  ArcAppReinstallSearchProvider(Profile* profile,
                                unsigned int max_result_count);

  ~ArcAppReinstallSearchProvider() override;

  // SearchProvider:
  void Start(const base::string16& query) override;

  // Used by unit tests. SearchProvider takes ownership of pointer.
  void SetTimerForTesting(std::unique_ptr<base::RepeatingTimer> timer);

  // ArcAppReinstallAppResult::Observer:
  void OnOpened(const std::string& package_name) override;

  void OnVisibilityChanged(const std::string& package_name,
                           bool visibility) override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);
 private:
  FRIEND_TEST_ALL_PREFIXES(::ArcAppReinstallSearchProviderTest,
                           TestResultsWithSearchChanged);
  FRIEND_TEST_ALL_PREFIXES(::ArcAppReinstallSearchProviderTest,
                           TestResultsWithAppsChanged);
  FRIEND_TEST_ALL_PREFIXES(::ArcAppReinstallSearchProviderTest,
                           TestResultListComparison);
  FRIEND_TEST_ALL_PREFIXES(::ArcAppReinstallSearchProviderTest,
                           TestShouldShowAnything);

  // Called to start fetching from our server for this result set. Called when
  // the play store becomes available.
  void StartFetch();

  void BeginRepeatingFetch();
  void StopRepeatingFetch();

  // Based on any change in results or query, updates the results appropriately.
  void UpdateResults();

  // If start_time is UnixEpoch, indicates a manual call.
  void OnGetAppReinstallCandidates(
      base::Time start_time,
      arc::mojom::AppReinstallState state,
      std::vector<arc::mojom::AppReinstallCandidatePtr> results);

  void MaybeUpdateFetching();

  // ArcAppListPrefs::Observer:
  // We listen for app state / registration for the play store, which is our
  // gate to turn on fetching.
  void OnAppRegistered(const std::string& app_id,
                       const ArcAppListPrefs::AppInfo& app_info) override;
  void OnAppStatesChanged(const std::string& app_id,
                          const ArcAppListPrefs::AppInfo& app_info) override;
  void OnAppRemoved(const std::string& id) override;
  void OnInstallationStarted(const std::string& package_name) override;
  void OnInstallationFinished(const std::string& package_name,
                              bool success) override;
  void OnPackageInstalled(
      const arc::mojom::ArcPackageInfo& package_info) override;
  void OnPackageRemoved(const std::string& package_name,
                        bool uninstalled) override;

  // For icon load callback, in OnGetAppReinstallCandidates
  void OnIconLoaded(const std::string& icon_url);

  // Should we show this package?
  bool ShouldShowPackage(const std::string& package_id) const;
  // Should we show anything to the user?
  bool ShouldShowAnything() const;

  // Are both lists of chrome search results of the same title in the same
  // order?
  static bool ResultsIdentical(
      const std::vector<std::unique_ptr<ChromeSearchResult>>& old_results,
      const std::vector<std::unique_ptr<ChromeSearchResult>>& new_results);

  Profile* const profile_;
  const unsigned int max_result_count_;
  const int icon_dimension_;

  // This is the latest loaded set of reinstallation candidates loaded from the
  // Play Store. This is used inside UpdateResults() to choose a subset of at
  // most max_result_count_ results, which are then shown to the user._
  std::vector<arc::mojom::AppReinstallCandidatePtr> loaded_value_;
  bool query_is_empty_ = true;

  // Repeating timer to fetch results.
  std::unique_ptr<base::RepeatingTimer> app_fetch_timer_;

  // Url to imageskia. This list is for icons that have been fully loaded.
  std::unordered_map<std::string, gfx::ImageSkia> icon_urls_;

  // url to imageskia of icons being loaded.
  std::unordered_map<std::string, gfx::ImageSkia> loading_icon_urls_;

  base::WeakPtrFactory<ArcAppReinstallSearchProvider> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(ArcAppReinstallSearchProvider);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_ARC_ARC_APP_REINSTALL_SEARCH_PROVIDER_H_
