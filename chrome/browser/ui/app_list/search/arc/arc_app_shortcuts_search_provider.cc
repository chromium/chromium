// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/arc/arc_app_shortcuts_search_provider.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/app_list/search/arc/arc_app_shortcut_search_result.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/session/arc_bridge_service.h"

namespace app_list {

ArcAppShortcutsSearchProvider::ArcAppShortcutsSearchProvider(
    int max_results,
    Profile* profile,
    AppListControllerDelegate* list_controller)
    : max_results_(max_results),
      profile_(profile),
      list_controller_(list_controller) {}

ArcAppShortcutsSearchProvider::~ArcAppShortcutsSearchProvider() = default;

void ArcAppShortcutsSearchProvider::Start(const base::string16& query) {
  arc::mojom::AppInstance* app_instance =
      arc::ArcServiceManager::Get()
          ? ARC_GET_INSTANCE_FOR_METHOD(
                arc::ArcServiceManager::Get()->arc_bridge_service()->app(),
                GetAppShortcutGlobalQueryItems)
          : nullptr;

  // TODO(931149): Currently we early-exit if the query is empty because we
  // don't show zero-state arc shortcuts. If this changes in future, remove this
  // early exit.
  if (!app_instance || query.empty()) {
    ClearResults();
    return;
  }

  if (query.empty()) {
    app_instance->GetAppShortcutGlobalQueryItems(
        base::UTF16ToUTF8(query), max_results_,
        base::BindOnce(&ArcAppShortcutsSearchProvider::UpdateRecommendedResults,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    // Invalidate the weak ptr to prevent previous callback run.
    weak_ptr_factory_.InvalidateWeakPtrs();
    app_instance->GetAppShortcutGlobalQueryItems(
        base::UTF16ToUTF8(query), max_results_,
        base::BindOnce(
            &ArcAppShortcutsSearchProvider::OnGetAppShortcutGlobalQueryItems,
            weak_ptr_factory_.GetWeakPtr()));
  }
}

void ArcAppShortcutsSearchProvider::UpdateRecommendedResults(
    std::vector<arc::mojom::AppShortcutItemPtr> shortcut_items) {
  const ArcAppListPrefs* arc_prefs = ArcAppListPrefs::Get(profile_);
  DCHECK(arc_prefs);

  // Maps app IDs to their score according to |ranker_|
  SearchProvider::Results search_results;
  for (auto& item : shortcut_items) {
    const std::string app_id =
        arc_prefs->GetAppIdByPackageName(item->package_name.value());
    std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
        arc_prefs->GetApp(app_id);
    // Ignore shortcuts for apps that are not present in the launcher.
    if (!app_info || !app_info->show_in_launcher)
      continue;
    auto result = std::make_unique<ArcAppShortcutSearchResult>(
        std::move(item), profile_, list_controller_,
        true /*is_recommendation*/);

    if (!app_info->install_time.is_null() ||
        !app_info->last_launch_time.is_null()) {
      // Case 1: It it has |install_time| or |last_launch_time|, set the
      // relevance to 0.5.
      result->set_relevance(0.5);
    } else {
      // Case 2: otherwise set relevance to 0.0.
      result->set_relevance(0);
    }
    search_results.emplace_back(std::move(result));
  }
  SwapResults(&search_results);
}

void ArcAppShortcutsSearchProvider::OnGetAppShortcutGlobalQueryItems(
    std::vector<arc::mojom::AppShortcutItemPtr> shortcut_items) {
  const ArcAppListPrefs* arc_prefs = ArcAppListPrefs::Get(profile_);
  DCHECK(arc_prefs);

  SearchProvider::Results search_results;
  for (auto& item : shortcut_items) {
    const std::string app_id =
        arc_prefs->GetAppIdByPackageName(item->package_name.value());
    std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
        arc_prefs->GetApp(app_id);
    // Ignore shortcuts for apps that are not present in the launcher.
    if (!app_info || !app_info->show_in_launcher)
      continue;
    search_results.emplace_back(std::make_unique<ArcAppShortcutSearchResult>(
        std::move(item), profile_, list_controller_,
        false /*is_recommendation*/));
  }
  SwapResults(&search_results);
}

}  // namespace app_list
