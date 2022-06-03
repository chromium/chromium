// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_result_ranker/histogram_util.h"

#include <cmath>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/containers/flat_set.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/ranking_item_util.h"

namespace app_list {
namespace {

ZeroStateResultType ZeroStateTypeFromRankingType(
    RankingItemType ranking_item_type) {
  switch (ranking_item_type) {
    case RankingItemType::kUnknown:
      return ZeroStateResultType::kUnknown;
    case RankingItemType::kIgnored:
    case RankingItemType::kFile:
    case RankingItemType::kApp:
    case RankingItemType::kArcAppShortcut:
      return ZeroStateResultType::kUnanticipated;
    case RankingItemType::kOmniboxGeneric:
      return ZeroStateResultType::kOmniboxSearch;
    case RankingItemType::kZeroStateFile:
    case RankingItemType::kZeroStateFileChip:
      return ZeroStateResultType::kZeroStateFile;
    case RankingItemType::kDriveQuickAccess:
    case RankingItemType::kDriveQuickAccessChip:
      return ZeroStateResultType::kDriveQuickAccess;
  }
}

}  // namespace

void LogInitializationStatus(const std::string& suffix,
                             InitializationStatus status) {
  if (suffix.empty())
    return;
  base::UmaHistogramEnumeration(
      "RecurrenceRanker.InitializationStatus." + suffix, status);
}

void LogSerializationStatus(const std::string& suffix,
                            SerializationStatus status) {
  if (suffix.empty())
    return;
  base::UmaHistogramEnumeration(
      "RecurrenceRanker.SerializationStatus." + suffix, status);
}

void LogUsage(const std::string& suffix, Usage usage) {
  if (suffix.empty())
    return;
  base::UmaHistogramEnumeration("RecurrenceRanker.Usage." + suffix, usage);
}

void LogJsonConfigConversionStatus(const std::string& suffix,
                                   JsonConfigConversionStatus status) {
  if (suffix.empty())
    return;
  base::UmaHistogramEnumeration(
      "RecurrenceRanker.JsonConfigConversion." + suffix, status);
}

void LogZeroStateLaunchType(RankingItemType ranking_item_type) {
  const auto zero_state_type = ZeroStateTypeFromRankingType(ranking_item_type);
  UMA_HISTOGRAM_ENUMERATION("Apps.AppList.ZeroStateResults.LaunchedItemType",
                            zero_state_type);
}

}  // namespace app_list
