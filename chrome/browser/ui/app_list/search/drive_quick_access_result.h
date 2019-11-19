// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_DRIVE_QUICK_ACCESS_RESULT_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_DRIVE_QUICK_ACCESS_RESULT_H_

#include "chrome/browser/ui/app_list/search/zero_state_file_result.h"

namespace app_list {

// A search result for a Drive QuickAccess file. This inherits from
// ZeroStateFileResult because all its logic is identical, but it is important
// they are different types for ranking purposes.
class DriveQuickAccessResult : public ZeroStateFileResult {
 public:
  DriveQuickAccessResult(const base::FilePath& filepath,
                         float relevance,
                         Profile* profile);

  // ZeroStateFileResult:
  ash::SearchResultType GetSearchResultType() const override;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_DRIVE_QUICK_ACCESS_RESULT_H_
