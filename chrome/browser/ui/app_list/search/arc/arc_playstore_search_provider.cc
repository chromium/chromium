// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/arc/arc_playstore_search_provider.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/extensions/gfx_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/search/arc/arc_playstore_search_result.h"
#include "components/arc/app/arc_playstore_search_request_state.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/session/arc_bridge_service.h"

namespace {
constexpr int kHistogramBuckets = 13;
constexpr char kAppListPlayStoreQueryStateHistogram[] =
    "Apps.AppListPlayStoreQueryState";

// Skips Play Store apps that have equivalent extensions installed.
// Do not skip recent instant apps since they should be treated like
// on-device apps.
bool CanSkipSearchResult(content::BrowserContext* context,
                         const arc::mojom::AppDiscoveryResult& result) {
  if (result.is_instant_app && result.is_recent)
    return false;

  if (!result.package_name.has_value())
    return false;

  if (!extensions::util::GetEquivalentInstalledExtensions(
           context, result.package_name.value())
           .empty()) {
    return true;
  }

  // TODO(crbug/763562): Remove this once we have a fix in Phonesky.
  // Don't show installed Android apps.
  const ArcAppListPrefs* arc_prefs = ArcAppListPrefs::Get(context);
  return arc_prefs && arc_prefs->GetPackage(result.package_name.value());
}

// Checks if we're receiving a result with invalid data from Android.
bool IsInvalidResult(const arc::mojom::AppDiscoveryResult& result) {
  // The result doesn't have a valid launch intent or install intent.
  if ((!result.launch_intent_uri || result.launch_intent_uri->empty()) &&
      (!result.install_intent_uri || result.install_intent_uri->empty())) {
    return true;
  }

  // The result doesn't have a valid label.
  if (!result.label || result.label->empty())
    return true;

  // The result doesn't have a valid formatted price.
  if (!result.formatted_price || result.formatted_price->empty())
    return true;

  // The result doesn't have a valid review score.
  if (result.review_score < 0)
    return true;

  // The result doesn't have a valid launcher icon.
  if (result.icon_png_data.empty())
    return true;

  // The result doesn't have a valid package name.
  if (!result.package_name || result.package_name->empty())
    return true;

  return false;
}
}  // namespace

namespace app_list {

ArcPlayStoreSearchProvider::ArcPlayStoreSearchProvider(
    int max_results,
    Profile* profile,
    AppListControllerDelegate* list_controller)
    : max_results_(max_results),
      profile_(profile),
      list_controller_(list_controller) {
  DCHECK_EQ(kHistogramBuckets, max_results + 1);
}

ArcPlayStoreSearchProvider::~ArcPlayStoreSearchProvider() = default;

void ArcPlayStoreSearchProvider::Start(const base::string16& query) {
  last_query_ = query;

  // Clear any results from the previous query.
  ClearResultsSilently();

  arc::mojom::AppInstance* app_instance =
      arc::ArcServiceManager::Get()
          ? ARC_GET_INSTANCE_FOR_METHOD(
                arc::ArcServiceManager::Get()->arc_bridge_service()->app(),
                GetRecentAndSuggestedAppsFromPlayStore)
          : nullptr;

  if (app_instance == nullptr || query.empty()) {
    return;
  }

  app_instance->GetRecentAndSuggestedAppsFromPlayStore(
      base::UTF16ToUTF8(query), max_results_,
      base::Bind(&ArcPlayStoreSearchProvider::OnResults,
                 weak_ptr_factory_.GetWeakPtr(), query,
                 base::TimeTicks::Now()));
}

void ArcPlayStoreSearchProvider::OnResults(
    const base::string16& query,
    base::TimeTicks query_start_time,
    arc::ArcPlayStoreSearchRequestState state,
    std::vector<arc::mojom::AppDiscoveryResultPtr> results) {
  if (state != arc::ArcPlayStoreSearchRequestState::SUCCESS) {
    DCHECK(results.empty());
    UMA_HISTOGRAM_ENUMERATION(kAppListPlayStoreQueryStateHistogram, state,
                              arc::ArcPlayStoreSearchRequestState::STATE_COUNT);
    return;
  }

  // Play store could have a long latency that when the results come back,
  // user has entered a different query. Do not return the staled results
  // from the previous query in such case.
  if (query != last_query_) {
    UMA_HISTOGRAM_ENUMERATION(
        kAppListPlayStoreQueryStateHistogram,
        arc::ArcPlayStoreSearchRequestState::FAILED_TO_CALL_CANCEL,
        arc::ArcPlayStoreSearchRequestState::STATE_COUNT);
    return;
  }

  SearchProvider::Results new_results;
  size_t instant_app_count = 0;
  for (auto& result : results) {
    if (IsInvalidResult(*result)) {
      UMA_HISTOGRAM_ENUMERATION(
          kAppListPlayStoreQueryStateHistogram,
          arc::ArcPlayStoreSearchRequestState::CHROME_GOT_INVALID_RESULT,
          arc::ArcPlayStoreSearchRequestState::STATE_COUNT);
      return;
    }

    if (result->is_instant_app)
      ++instant_app_count;

    if (CanSkipSearchResult(profile_, *result))
      continue;

    new_results.emplace_back(std::make_unique<ArcPlayStoreSearchResult>(
        std::move(result), profile_, list_controller_));
  }
  SwapResults(&new_results);

  // Record user metrics.
  UMA_HISTOGRAM_ENUMERATION(kAppListPlayStoreQueryStateHistogram,
                            arc::ArcPlayStoreSearchRequestState::SUCCESS,
                            arc::ArcPlayStoreSearchRequestState::STATE_COUNT);
  UMA_HISTOGRAM_TIMES("Arc.PlayStoreSearch.QueryTime",
                      base::TimeTicks::Now() - query_start_time);
  if (results.size() > 0) {
    UMA_HISTOGRAM_EXACT_LINEAR("Arc.PlayStoreSearch.ReturnedAppsTotal",
                               results.size(), kHistogramBuckets);
  }
  if (results.size() - instant_app_count > 0) {
    UMA_HISTOGRAM_EXACT_LINEAR("Arc.PlayStoreSearch.ReturnedUninstalledApps",
                               results.size() - instant_app_count,
                               kHistogramBuckets);
  }
  if (instant_app_count > 0) {
    UMA_HISTOGRAM_EXACT_LINEAR("Arc.PlayStoreSearch.ReturnedInstantApps",
                               instant_app_count, kHistogramBuckets);
  }
}

}  // namespace app_list
