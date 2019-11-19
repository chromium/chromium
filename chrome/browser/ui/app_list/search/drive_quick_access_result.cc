// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/drive_quick_access_result.h"

#include <utility>

#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "chrome/browser/profiles/profile.h"

namespace app_list {
namespace {

const char kDriveQuickAccessResultPrefix[] = "quickaccess://";

}

DriveQuickAccessResult::DriveQuickAccessResult(const base::FilePath& filepath,
                                               float relevance,
                                               Profile* profile)
    : ZeroStateFileResult(filepath, relevance, profile) {
  set_id(kDriveQuickAccessResultPrefix + filepath.value());
  SetResultType(ResultType::kDriveQuickAccess);
}

ash::SearchResultType DriveQuickAccessResult::GetSearchResultType() const {
  return ash::DRIVE_QUICK_ACCESS;
}

}  // namespace app_list
