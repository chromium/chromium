// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/ranking/util.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/ranking/constants.h"

namespace app_list {
namespace {

// The directory within the cryptohome to save ranking state into.
constexpr char kRankerStateDirectory[] = "launcher_ranking/";

}  // namespace

base::FilePath RankerStateDirectory(Profile* profile) {
  return profile->GetPath().AppendASCII(kRankerStateDirectory);
}

Category ResultTypeToCategory(ResultType result_type) {
  switch (result_type) {
    case ResultType::kInstalledApp:
    case ResultType::kInstantApp:
    case ResultType::kInternalApp:
    case ResultType::kArcAppShortcut:
      return Category::kApp;
    case ResultType::kOmnibox:
    case ResultType::kAnswerCard:
      return Category::kWeb;
    case ResultType::kZeroStateFile:
    case ResultType::kZeroStateDrive:
    case ResultType::kFileChip:
    case ResultType::kDriveChip:
    case ResultType::kFileSearch:
    case ResultType::kDriveSearch:
      return Category::kFiles;
    case ResultType::kAssistantChip:
    case ResultType::kAssistantText:
      return Category::kAssistant;
    case ResultType::kOsSettings:
      return Category::kSettings;
    case ResultType::kHelpApp:
      return Category::kHelp;
    case ResultType::kPlayStoreReinstallApp:
    case ResultType::kPlayStoreApp:
      return Category::kPlayStore;
    // Never used in the search backend.
    case ResultType::kUnknown:
    // Suggested content toggle fake result type. Used only in ash, not in the
    // search backend.
    case ResultType::kInternalPrivacyInfo:
    // Deprecated.
    case ResultType::kLauncher:
      NOTREACHED();
      return Category::kApp;
  }
}

std::u16string CategoryDebugString(const Category category) {
  switch (category) {
    case Category::kApp:
      return u"(apps) ";
    case Category::kWeb:
      return u"(web) ";
    case Category::kFiles:
      return u"(files) ";
    case Category::kAssistant:
      return u"(assistant) ";
    case Category::kSettings:
      return u"(settings) ";
    case Category::kHelp:
      return u"(help) ";
    case Category::kPlayStore:
      return u"(play store) ";
  }
}

std::u16string RemoveDebugPrefix(const std::u16string str) {
  std::string result = base::UTF16ToUTF8(str);

  if (result.empty() || result[0] != '(')
    return str;

  const std::size_t delimiter_index = result.find(") ");
  if (delimiter_index != std::string::npos)
    result.erase(0, delimiter_index + 2);
  return base::UTF8ToUTF16(result);
}

std::u16string RemoveTopMatchPrefix(const std::u16string str) {
  const std::string top_match_details = kTopMatchDetails;
  std::string result = base::UTF16ToUTF8(str);

  if (result.empty() || result.rfind(top_match_details, 0u) != 0)
    return str;

  result.erase(0, top_match_details.size());
  return base::UTF8ToUTF16(result);
}

}  // namespace app_list
