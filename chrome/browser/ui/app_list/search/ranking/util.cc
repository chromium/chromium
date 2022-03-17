// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/ranking/util.h"

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/profiles/profile.h"

namespace app_list {
namespace {

// The directory within the cryptohome to save ranking state into.
constexpr char kRankerStateDirectory[] = "launcher_ranking/";

}  // namespace

base::FilePath RankerStateDirectory(Profile* profile) {
  return profile->GetPath().AppendASCII(kRankerStateDirectory);
}

std::string CategoryToString(const Category value) {
  return base::NumberToString(static_cast<int>(value));
}

Category StringToCategory(const std::string& value) {
  int number;
  base::StringToInt(value, &number);
  return static_cast<Category>(number);
}

Category ResultTypeToCategory(ResultType result_type) {
  switch (result_type) {
    case ResultType::kInstalledApp:
    case ResultType::kInstantApp:
    case ResultType::kInternalApp:
    case ResultType::kGames:
      return Category::kApps;
    case ResultType::kArcAppShortcut:
      return Category::kAppShortcuts;
    case ResultType::kOmnibox:
    case ResultType::kAnswerCard:
    case ResultType::kOpenTab:
      return Category::kWeb;
    case ResultType::kZeroStateFile:
    case ResultType::kZeroStateDrive:
    case ResultType::kFileChip:
    case ResultType::kDriveChip:
    case ResultType::kFileSearch:
    case ResultType::kDriveSearch:
      return Category::kFiles;
    case ResultType::kOsSettings:
      return Category::kSettings;
    case ResultType::kHelpApp:
    case ResultType::kKeyboardShortcut:
      return Category::kHelp;
    case ResultType::kPlayStoreReinstallApp:
    case ResultType::kPlayStoreApp:
      return Category::kPlayStore;
    case ResultType::kAssistantChip:
    case ResultType::kAssistantText:
      return Category::kSearchAndAssistant;
    // Never used in the search backend.
    case ResultType::kUnknown:
    // Suggested content toggle fake result type. Used only in ash, not in the
    // search backend.
    case ResultType::kInternalPrivacyInfo:
    // Deprecated.
    case ResultType::kLauncher:
      return Category::kUnknown;
  }
}

}  // namespace app_list
