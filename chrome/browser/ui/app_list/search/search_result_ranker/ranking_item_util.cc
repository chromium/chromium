// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_result_ranker/ranking_item_util.h"

#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/macros.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ui/app_list/chrome_app_list_item.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "components/omnibox/browser/autocomplete_match_type.h"

namespace app_list {

RankingItemType RankingItemTypeFromSearchResult(
    const ChromeSearchResult& result) {
  switch (result.result_type()) {
    case ash::AppListSearchResultType::kInstalledApp:
    case ash::AppListSearchResultType::kInternalApp:
      return RankingItemType::kApp;
    case ash::AppListSearchResultType::kOmnibox:
      return RankingItemType::kOmniboxGeneric;
    case ash::AppListSearchResultType::kLauncher:
    case ash::AppListSearchResultType::kFileSearch:
    case ash::AppListSearchResultType::kDriveSearch:
      return RankingItemType::kFile;
    case ash::AppListSearchResultType::kUnknown:
    case ash::AppListSearchResultType::kPlayStoreApp:
    case ash::AppListSearchResultType::kInstantApp:
    case ash::AppListSearchResultType::kAnswerCard:
    case ash::AppListSearchResultType::kPlayStoreReinstallApp:
    case ash::AppListSearchResultType::kAssistantChip:
    case ash::AppListSearchResultType::kOsSettings:
    case ash::AppListSearchResultType::kInternalPrivacyInfo:
    case ash::AppListSearchResultType::kAssistantText:
    case ash::AppListSearchResultType::kHelpApp:
      // NOTE: We don't rank results of type kAssistantChip, kAssistantText
      // as those results, if present, are shown in a dedicated slot.
      return RankingItemType::kIgnored;
    case ash::AppListSearchResultType::kArcAppShortcut:
      return RankingItemType::kArcAppShortcut;
    case ash::AppListSearchResultType::kZeroStateFile:
      return RankingItemType::kZeroStateFile;
    case ash::AppListSearchResultType::kZeroStateDrive:
      return RankingItemType::kDriveQuickAccess;
    case ash::AppListSearchResultType::kFileChip:
      return RankingItemType::kZeroStateFileChip;
    case ash::AppListSearchResultType::kDriveChip:
      return RankingItemType::kDriveQuickAccessChip;
  }
}

RankingItemType RankingItemTypeFromChromeAppListItem(
    const ChromeAppListItem& item) {
  return RankingItemType::kApp;
}

std::string NormalizeAppId(const std::string& id) {
  std::string app_id(id);
  // No existing scheme names include the delimiter string "://".
  std::size_t delimiter_index = app_id.find("://");
  if (delimiter_index != std::string::npos)
    app_id.erase(0, delimiter_index + 3);
  if (!app_id.empty() && app_id.back() == '/')
    app_id.pop_back();
  return app_id;
}

std::string SimplifyUrlId(const std::string& url_id) {
  std::string result(url_id);

  std::size_t query_index = result.find("?");
  if (query_index != std::string::npos)
    result.erase(query_index);

  std::size_t fragment_index = result.find("#");
  if (fragment_index != std::string::npos)
    result.erase(fragment_index);

  if (!result.empty() && result.back() == '/' &&
      result.find("://") != result.size() - 3)
    result.pop_back();

  return result;
}

std::string SimplifyGoogleDocsUrlId(const std::string& url_id) {
  std::string result = SimplifyUrlId(url_id);

  // URLs that end with /view or /edit point to the same document, so should be
  // the same for ranking purposes.
  if (base::EndsWith(result, "/view", base::CompareCase::INSENSITIVE_ASCII) ||
      base::EndsWith(result, "/edit", base::CompareCase::INSENSITIVE_ASCII)) {
    result = result.substr(0, result.length() - 5);
  }

  return result;
}

}  // namespace app_list
