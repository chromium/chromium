// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/common/types_util.h"

namespace app_list {

std::string ResultTypeToString(const ash::AppListSearchResultType result_type) {
  switch (result_type) {
    case ash::AppListSearchResultType::kUnknown:
      return "Unknown";
    case ash::AppListSearchResultType::kInstalledApp:
      return "Installed app";
    case ash::AppListSearchResultType::kPlayStoreApp:
      return "Play store app";
    case ash::AppListSearchResultType::kInstantApp:
      return "Instant app";
    case ash::AppListSearchResultType::kInternalApp:
      return "Internal app";
    case ash::AppListSearchResultType::kOmnibox:
      return "Omnibox";
    case ash::AppListSearchResultType::kLauncher:
      return "Launcher (files)";
    case ash::AppListSearchResultType::kAnswerCard:
      return "Answer card";
    case ash::AppListSearchResultType::kPlayStoreReinstallApp:
      return "Play store reinstall app";
    case ash::AppListSearchResultType::kArcAppShortcut:
      return "Arc app shortcut";
    case ash::AppListSearchResultType::kZeroStateFile:
      return "Zero state file";
    case ash::AppListSearchResultType::kZeroStateDrive:
      return "Zero state drive";
    case ash::AppListSearchResultType::kFileChip:
      return "File chip";
    case ash::AppListSearchResultType::kDriveChip:
      return "Drive chip";
    case ash::AppListSearchResultType::kAssistantChip:
      return "Assistant chip";
    case ash::AppListSearchResultType::kOsSettings:
      return "OS settings";
    case ash::AppListSearchResultType::kInternalPrivacyInfo:
      return "Internal privacy info";
    case ash::AppListSearchResultType::kAssistantText:
      return "Assistant text";
    case ash::AppListSearchResultType::kHelpApp:
      return "Help app";
    case ash::AppListSearchResultType::kFileSearch:
      return "File search";
    case ash::AppListSearchResultType::kDriveSearch:
      return "Drive search";
  }
  NOTREACHED();
}

std::string DisplayTypeToString(
    const ash::SearchResultDisplayType display_type) {
  switch (display_type) {
    case ash::SearchResultDisplayType::kNone:
    case ash::SearchResultDisplayType::kLast:
      return "None";
    case ash::SearchResultDisplayType::kList:
      return "List";
    case ash::SearchResultDisplayType::kTile:
      return "Tile";
    case ash::SearchResultDisplayType::kCard:
      return "Card";
    case ash::SearchResultDisplayType::kChip:
      return "Chip";
  }
  NOTREACHED();
}

}  // namespace app_list
