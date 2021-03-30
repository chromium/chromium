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
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/settings/chromeos/hierarchy.h"
#include "chrome/browser/ui/webui/settings/chromeos/os_settings_manager.h"
#include "chrome/browser/ui/webui/settings/chromeos/os_settings_manager_factory.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/search_handler.h"
#include "chrome/browser/web_applications/components/web_app_id_constants.h"
#include "chrome/common/chrome_features.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace app_list {
namespace {

using SettingsResultPtr = chromeos::settings::mojom::SearchResultPtr;
using SettingsResultType = chromeos::settings::mojom::SearchResultType;
using Setting = chromeos::settings::mojom::Setting;
using Subpage = chromeos::settings::mojom::Subpage;
using Section = chromeos::settings::mojom::Section;

constexpr char kOsSettingsResultPrefix[] = "os-settings://";
constexpr float kScoreEps = 1e-5f;

constexpr size_t kNumRequestedResults = 5u;
constexpr size_t kMaxShownResults = 2u;

// Various error states of the OsSettingsProvider. kOk is currently not emitted,
// but may be used in future. These values persist to logs. Entries should not
// be renumbered and numeric values should never be reused.
enum class Error {
  kOk = 0,
  // No longer used.
  // kAppServiceUnavailable = 1,
  kNoSettingsIcon = 2,
  kSearchHandlerUnavailable = 3,
  kHierarchyEmpty = 4,
  kNoHierarchy = 5,
  kSettingsAppNotReady = 6,
  kMaxValue = kSettingsAppNotReady,
};

void LogError(Error error) {
  UMA_HISTOGRAM_ENUMERATION("Apps.AppList.OsSettingsProvider.Error", error);
}

bool ContainsAncestor(Subpage subpage,
                      const chromeos::settings::Hierarchy* hierarchy,
                      const base::flat_set<Subpage>& subpages,
                      const base::flat_set<Section>& sections) {
  // Returns whether or not an ancestor subpage or section of |subpage| is
  // present within |subpages| or |sections|.
  const auto& metadata = hierarchy->GetSubpageMetadata(subpage);

  // Check parent subpage if one exists.
  if (metadata.parent_subpage) {
    const auto it = subpages.find(metadata.parent_subpage);
    if (it != subpages.end() ||
        ContainsAncestor(metadata.parent_subpage.value(), hierarchy, subpages,
                         sections))
      return true;
  }

  // Check section.
  const auto it = sections.find(metadata.section);
  return it != sections.end();
}

bool ContainsAncestor(Setting setting,
                      const chromeos::settings::Hierarchy* hierarchy,
                      const base::flat_set<Subpage>& subpages,
                      const base::flat_set<Section>& sections) {
  // Returns whether or not an ancestor subpage or section of |setting| is
  // present within |subpages| or |sections|.
  const auto& metadata = hierarchy->GetSettingMetadata(setting);

  // Check primary subpage only. Alternate subpages aren't used enough for the
  // check to be worthwhile.
  if (metadata.primary.second) {
    const auto parent_subpage = metadata.primary.second.value();
    const auto it = subpages.find(parent_subpage);
    if (it != subpages.end() ||
        ContainsAncestor(parent_subpage, hierarchy, subpages, sections))
      return true;
  }

  // Check section.
  const auto it = sections.find(metadata.primary.first);
  return it != sections.end();
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
  SetMetricsType(ash::OS_SETTINGS);
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

  // Manually build the accessible name for the search result, in a way that
  // parallels the regular accessible names set by
  // SearchResultBaseView::ComputeAccessibleName.
  std::u16string accessible_name = title();
  if (!details().empty()) {
    accessible_name += u", ";
    accessible_name += details();
  }
  accessible_name += u", ";
  // The first element in the settings hierarchy is always the top-level
  // localized name of the Settings app.
  accessible_name += hierarchy[0];
  SetAccessibleName(accessible_name);
}

OsSettingsResult::~OsSettingsResult() = default;

void OsSettingsResult::Open(int event_flags) {
  chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(profile_,
                                                               url_path_);
}

OsSettingsProvider::OsSettingsProvider(Profile* profile)
    : profile_(profile),
      settings_manager_(
          chromeos::settings::OsSettingsManagerFactory::GetForProfile(
              profile)) {
  DCHECK(profile_);

  if (settings_manager_) {
    search_handler_ = settings_manager_->search_handler();
    hierarchy_ = settings_manager_->hierarchy();
  }

  // |search_handler_| can be nullptr in the case that the new OS settings
  // search chrome flag is disabled. If it is, we should effectively disable the
  // search provider.
  if (!search_handler_) {
    LogError(Error::kSearchHandlerUnavailable);
    return;
  }

  if (!hierarchy_) {
    LogError(Error::kNoHierarchy);
  }

  search_handler_->Observe(
      search_results_observer_receiver_.BindNewPipeAndPassRemote());

  app_service_proxy_ = apps::AppServiceProxyFactory::GetForProfile(profile_);
  DCHECK(app_service_proxy_);

  Observe(&app_service_proxy_->AppRegistryCache());
  auto icon_type =
      (base::FeatureList::IsEnabled(features::kAppServiceAdaptiveIcon))
          ? apps::mojom::IconType::kStandard
          : apps::mojom::IconType::kUncompressed;
  app_service_proxy_->LoadIcon(
      apps::mojom::AppType::kWeb, web_app::kOsSettingsAppId, icon_type,
      ash::SharedAppListConfig::instance().search_list_icon_dimension(),
      /*allow_placeholder_icon=*/false,
      base::BindOnce(&OsSettingsProvider::OnLoadIcon,
                     weak_factory_.GetWeakPtr()));

  // Set parameters from Finch. Reasonable defaults are set in the header.
  accept_alternate_matches_ = base::GetFieldTrialParamByFeatureAsBool(
      app_list_features::kLauncherSettingsSearch, "accept_alternate_matches",
      accept_alternate_matches_);
  min_query_length_ = base::GetFieldTrialParamByFeatureAsInt(
      app_list_features::kLauncherSettingsSearch, "min_query_length",
      min_query_length_);
  min_query_length_for_alternates_ = base::GetFieldTrialParamByFeatureAsInt(
      app_list_features::kLauncherSettingsSearch,
      "min_query_length_for_alternates", min_query_length_for_alternates_);
  min_score_ = base::GetFieldTrialParamByFeatureAsDouble(
      app_list_features::kLauncherSettingsSearch, "min_score", min_score_);
  min_score_for_alternates_ = base::GetFieldTrialParamByFeatureAsDouble(
      app_list_features::kLauncherSettingsSearch, "min_score_for_alternates",
      min_score_for_alternates_);
}

OsSettingsProvider::~OsSettingsProvider() = default;

ash::AppListSearchResultType OsSettingsProvider::ResultType() {
  return ash::AppListSearchResultType::kOsSettings;
}

void OsSettingsProvider::Start(const std::u16string& query) {
  const base::TimeTicks start_time = base::TimeTicks::Now();
  last_query_ = query;
  // Disable the provider if:
  //  - the search backend isn't available
  //  - the settings app isn't ready
  //  - we don't have an icon to display with results.
  if (!search_handler_) {
    return;
  } else if (icon_.isNull()) {
    LogError(Error::kNoSettingsIcon);
    return;
  }

  ClearResultsSilently();

  // Do not return results for queries that are too short, as the results
  // generally aren't meaningful. Note this provider never provides zero-state
  // results.
  if (query.size() < min_query_length_)
    return;

  // Invalidate weak pointers to cancel existing searches.
  weak_factory_.InvalidateWeakPtrs();
  search_handler_->Search(
      query, kNumRequestedResults,
      chromeos::settings::mojom::ParentResultBehavior::
          kDoNotIncludeParentResults,
      base::BindOnce(&OsSettingsProvider::OnSearchReturned,
                     weak_factory_.GetWeakPtr(), query, start_time));
}

void OsSettingsProvider::ViewClosing() {
  last_query_.clear();
}

void OsSettingsProvider::OnSearchReturned(
    const std::u16string& query,
    const base::TimeTicks& start_time,
    std::vector<chromeos::settings::mojom::SearchResultPtr> sorted_results) {
  // TODO(crbug.com/1068851): We are currently not ranking settings results.
  // Instead, we are gluing at most two to the top of the search box. Consider
  // ranking these with other results in the next version of the feature.
  DCHECK_LE(sorted_results.size(), kNumRequestedResults);

  SearchProvider::Results search_results;
  int i = 0;
  for (const auto& result : FilterResults(query, sorted_results, hierarchy_)) {
    const float score = 1.0f - i * kScoreEps;
    search_results.emplace_back(
        std::make_unique<OsSettingsResult>(profile_, result, score, icon_));
    ++i;
  }

  UMA_HISTOGRAM_TIMES("Apps.AppList.OsSettingsProvider.QueryTime",
                      base::TimeTicks::Now() - start_time);
  SwapResults(&search_results);
}

void OsSettingsProvider::OnAppUpdate(const apps::AppUpdate& update) {
  if (update.AppId() != web_app::kOsSettingsAppId)
    return;

  // TODO(crbug.com/1068851): We previously disabled this search provider until
  // the app service signalled that the settings app is ready. But this signal
  // is flaky, so sometimes search provider was permanently disabled. Once the
  // signal is reliable, we should re-add the check.

  // Request the Settings app icon when either the readiness or the icon has
  // changed.
  if (update.ReadinessChanged() || update.IconKeyChanged()) {
    auto icon_type =
        (base::FeatureList::IsEnabled(features::kAppServiceAdaptiveIcon))
            ? apps::mojom::IconType::kStandard
            : apps::mojom::IconType::kUncompressed;
    app_service_proxy_->LoadIcon(
        apps::mojom::AppType::kWeb, web_app::kOsSettingsAppId, icon_type,
        ash::SharedAppListConfig::instance().search_list_icon_dimension(),
        /*allow_placeholder_icon=*/false,
        base::BindOnce(&OsSettingsProvider::OnLoadIcon,
                       weak_factory_.GetWeakPtr()));
  }
}

void OsSettingsProvider::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  Observe(nullptr);
}

void OsSettingsProvider::OnSearchResultAvailabilityChanged() {
  if (last_query_.empty())
    return;

  Start(last_query_);
}

std::vector<chromeos::settings::mojom::SearchResultPtr>
OsSettingsProvider::FilterResults(
    const std::u16string& query,
    const std::vector<chromeos::settings::mojom::SearchResultPtr>& results,
    const chromeos::settings::Hierarchy* hierarchy) {
  base::flat_set<std::string> seen_urls;
  base::flat_set<Subpage> seen_subpages;
  base::flat_set<Section> seen_sections;
  std::vector<SettingsResultPtr> clean_results;

  for (const SettingsResultPtr& result : results) {
    // Filter results below the score threshold.
    if (result->relevance_score < min_score_) {
      continue;
    }

    // Check if query matched alternate text for the result. If so, only allow
    // results meeting extra requirements. Perform this check before checking
    // for duplicates to ensure a rejected alternate result doesn't preclude a
    // canonical result with a lower score from being shown.
    if (result->result_text != result->canonical_result_text &&
        (!accept_alternate_matches_ ||
         query.size() < min_query_length_for_alternates_ ||
         result->relevance_score < min_score_for_alternates_)) {
      continue;
    }

    // Check if URL has been seen.
    const std::string url = result->url_path_with_parameters;
    const auto it = seen_urls.find(url);
    if (it != seen_urls.end())
      continue;

    seen_urls.insert(url);
    clean_results.push_back(result.Clone());
    if (result->type == SettingsResultType::kSubpage)
      seen_subpages.insert(result->id->get_subpage());
    if (result->type == SettingsResultType::kSection)
      seen_sections.insert(result->id->get_section());
  }

  // Iterate through the clean results a second time. Remove subpage or setting
  // results that have an ancestor subpage or section also present in the
  // results.
  for (size_t i = 0; i < clean_results.size(); ++i) {
    const auto& result = clean_results[i];
    if ((result->type == SettingsResultType::kSubpage &&
         ContainsAncestor(result->id->get_subpage(), hierarchy_, seen_subpages,
                          seen_sections)) ||
        (result->type == SettingsResultType::kSetting &&
         ContainsAncestor(result->id->get_setting(), hierarchy_, seen_subpages,
                          seen_sections))) {
      clean_results.erase(clean_results.begin() + i);
      --i;
    }
  }

  if (clean_results.size() > static_cast<size_t>(kMaxShownResults))
    clean_results.resize(kMaxShownResults);
  return clean_results;
}

void OsSettingsProvider::OnLoadIcon(apps::mojom::IconValuePtr icon_value) {
  if (icon_value.is_null())
    return;

  auto icon_type =
      (base::FeatureList::IsEnabled(features::kAppServiceAdaptiveIcon))
          ? apps::mojom::IconType::kStandard
          : apps::mojom::IconType::kUncompressed;
  if (icon_value->icon_type == icon_type) {
    icon_ = icon_value->uncompressed;
  }
}

}  // namespace app_list
