// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/ranking/types.h"

namespace app_list {

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

}  // namespace app_list
