// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/os_settings_provider.h"

#include <algorithm>
#include <memory>
#include <string>

#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/chromeos/web_applications/default_web_app_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/settings/chromeos/os_settings_manager.h"
#include "chrome/browser/ui/webui/settings/chromeos/os_settings_manager_factory.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/search.mojom.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/search_handler.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace app_list {
namespace {

using SettingsResultPtr = chromeos::settings::mojom::SearchResultPtr;
using SettingsResultType = chromeos::settings::mojom::SearchResultType;

constexpr char kOsSettingsResultPrefix[] = "os-settings://";
constexpr float kScoreEps = 1e-5f;

// TODO(crbug.com/1068851): We may want to control some of these constants via
// Finch.
constexpr size_t kMinQueryLength = 1u;
constexpr size_t kMinQueryLengthForAlternates = 4u;
constexpr float kMinScoreForAlternates = 0.3f;

constexpr size_t kNumRequestedResults = 5u;
constexpr size_t kMaxShownResults = 2u;

// Various error states of the OsSettingsProvider. kOk is currently not emitted,
// but may be used in future. These values persist to logs. Entries should not
// be renumbered and numeric values should never be reused.
enum class Error {
  kOk = 0,
  kAppServiceUnavailable = 1,
  kNoSettingsIcon = 2,
  kSearchHandlerUnavailable = 3,
  kHierarchyEmpty = 4,
  kMaxValue = kHierarchyEmpty
};

void LogError(Error error) {
  UMA_HISTOGRAM_ENUMERATION("Apps.AppList.OsSettingsProvider.Error", error);
}

// Given a vector of results from the SearchHandler, filters them down to a
// display-ready vector. It:
// - returns at most |kMaxShownResults| results
// - removes results with duplicate IDs
// - removes results matching alternate text unless they meet extra requirements
//
// The SearchHandler's vector is ranked high-to-low with this logic:
// - compares SearchResultDefaultRank,
// - if equal, compares relevance scores
// - if equal, compares SearchResultType, favoring sections over subpages over
//   settings
// - if equal, picks arbitrarily
//
// So simply iterating down the vector while being careful about duplicates and
// checking for alternate matches is enough.
std::vector<SettingsResultPtr> FilterResults(
    const base::string16& query,
    const std::vector<SettingsResultPtr>& results) {
  base::flat_set<std::string> seen_urls;
  std::vector<SettingsResultPtr> clean_results;

  for (const SettingsResultPtr& result : results) {
    // Check if query matched alternate text for the result. If so, only allow
    // results meeting extra requirements. Perform this check before checking
    // for duplicates to ensure a rejected alternate result doesn't preclude a
    // canonical result with a lower score from being shown.
    if (result->result_text != result->canonical_result_text &&
        (query.size() < kMinQueryLengthForAlternates ||
         result->relevance_score < kMinScoreForAlternates)) {
      continue;
    }

    // Check if URL has been seen.
    const std::string url = result->url_path_with_parameters;
    const auto it = seen_urls.find(url);
    if (it != seen_urls.end())
      continue;
    seen_urls.insert(url);

    clean_results.push_back(result.Clone());
    if (clean_results.size() == kMaxShownResults)
      break;
  }

  return clean_results;
}

}  // namespace

OsSettingsResult::OsSettingsResult(
    Profile* profile,
    const chromeos::settings::mojom::SearchResultPtr& result,
    const float relevance_score,
    const gfx::ImageSkia& icon)
    : profile_(profile), url_path_(result->url_path_with_parameters) {
  set_id(kOsSettingsResultPrefix + url_path_);
  set_relevance(relevance_score);
  SetTitle(result->canonical_result_text);
  SetResultType(ResultType::kOsSettings);
  SetDisplayType(DisplayType::kList);
  SetIcon(icon);

  // If the result is not a top-level section, set the display text with
  // information about the result's 'parent' category. This is the last element
  // of |result->settings_page_hierarchy|, which is localized and ready for
  // display. Some subpages have the same name as their section (namely,
  // bluetooth), in which case we should leave the details blank.
  const auto& hierarchy = result->settings_page_hierarchy;
  if (hierarchy.empty()) {
    LogError(Error::kHierarchyEmpty);
  } else if (result->type != SettingsResultType::kSection) {
    SetDetails(hierarchy.back());
  }
}

OsSettingsResult::~OsSettingsResult() = default;

void OsSettingsResult::Open(int event_flags) {
  chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(profile_,
                                                               url_path_);
}

ash::SearchResultType OsSettingsResult::GetSearchResultType() const {
  return ash::OS_SETTINGS;
}

OsSettingsProvider::OsSettingsProvider(Profile* profile)
    : profile_(profile),
      search_handler_(
          chromeos::settings::OsSettingsManagerFactory::GetForProfile(profile)
              ->search_handler()) {
  DCHECK(profile_);

  // |search_handler_| can be nullptr in the case that the new OS settings
  // search chrome flag is disabled. If it is, we should effectively disable the
  // search provider.
  if (!search_handler_) {
    LogError(Error::kSearchHandlerUnavailable);
    return;
  }

  app_service_proxy_ = apps::AppServiceProxyFactory::GetForProfile(profile_);
  if (app_service_proxy_) {
    Observe(&app_service_proxy_->AppRegistryCache());
    app_service_proxy_->LoadIcon(
        apps::mojom::AppType::kWeb,
        chromeos::default_web_apps::kOsSettingsAppId,
        apps::mojom::IconCompression::kUncompressed,
        ash::AppListConfig::instance().search_list_icon_dimension(),
        /*allow_placeholder_icon=*/false,
        base::BindOnce(&OsSettingsProvider::OnLoadIcon,
                       weak_factory_.GetWeakPtr()));
  } else {
    LogError(Error::kAppServiceUnavailable);
  }
}

OsSettingsProvider::~OsSettingsProvider() = default;

ash::AppListSearchResultType OsSettingsProvider::ResultType() {
  return ash::AppListSearchResultType::kOsSettings;
}

void OsSettingsProvider::Start(const base::string16& query) {
  if (!search_handler_)
    return;

  ClearResultsSilently();

  // This provider does not handle zero-state, and shouldn't return any results
  // for a single-character query because they aren't meaningful enough.
  if (query.size() <= kMinQueryLength)
    return;

  // Invalidate weak pointers to cancel existing searches.
  weak_factory_.InvalidateWeakPtrs();
  search_handler_->Search(query, kNumRequestedResults,
                          chromeos::settings::mojom::ParentResultBehavior::
                              kDoNotIncludeParentResults,
                          base::BindOnce(&OsSettingsProvider::OnSearchReturned,
                                         weak_factory_.GetWeakPtr(), query));
}

void OsSettingsProvider::OnSearchReturned(
    const base::string16& query,
    std::vector<chromeos::settings::mojom::SearchResultPtr> sorted_results) {
  // TODO(crbug.com/1068851): We are currently not ranking settings results.
  // Instead, we are gluing at most two to the top of the search box. Consider
  // ranking these with other results in the next version of the feature.
  DCHECK_LE(sorted_results.size(), kNumRequestedResults);
  if (icon_.isNull())
    LogError(Error::kNoSettingsIcon);

  SearchProvider::Results search_results;
  int i = 0;
  for (const auto& result : FilterResults(query, sorted_results)) {
    const float score = 1.0f - i * kScoreEps;
    search_results.emplace_back(
        std::make_unique<OsSettingsResult>(profile_, result, score, icon_));
    ++i;
  }

  SwapResults(&search_results);
}

void OsSettingsProvider::OnAppUpdate(const apps::AppUpdate& update) {
  // Watch the app service for updates. On an update that marks the OS settings
  // app as ready, retrieve the icon for the app to use for search results.
  if (app_service_proxy_ &&
      update.AppId() == chromeos::default_web_apps::kOsSettingsAppId &&
      update.ReadinessChanged() &&
      update.Readiness() == apps::mojom::Readiness::kReady) {
    app_service_proxy_->LoadIcon(
        apps::mojom::AppType::kWeb,
        chromeos::default_web_apps::kOsSettingsAppId,
        apps::mojom::IconCompression::kUncompressed,
        ash::AppListConfig::instance().search_list_icon_dimension(),
        /*allow_placeholder_icon=*/false,
        base::BindOnce(&OsSettingsProvider::OnLoadIcon,
                       weak_factory_.GetWeakPtr()));
  }
}

void OsSettingsProvider::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  Observe(nullptr);
}

void OsSettingsProvider::OnLoadIcon(apps::mojom::IconValuePtr icon_value) {
  if (icon_value->icon_compression ==
      apps::mojom::IconCompression::kUncompressed) {
    icon_ = icon_value->uncompressed;
  }
}

}  // namespace app_list
