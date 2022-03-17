// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_UTIL_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_UTIL_H_

#include <string>

#include "base/files/file_path.h"
#include "chrome/browser/ui/app_list/search/ranking/types.h"

class Profile;

namespace app_list {

// Returns the absolute path of the directory rankers should serialize their
// state into.
base::FilePath RankerStateDirectory(Profile* profile);

std::string CategoryToString(const Category value);

Category StringToCategory(const std::string& value);

// TODO(crbug.com/1199206): This can be removed once LaunchData contains the
// result category.
//
// This is slightly inconsistent with the true result->category mapping, because
// Omnibox results can either be in the kWeb or kSearchAndAssistant category.
Category ResultTypeToCategory(ResultType result_type);

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_UTIL_H_
