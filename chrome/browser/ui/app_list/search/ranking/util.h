// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_UTIL_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_UTIL_H_

#include <string>

#include "base/files/file_path.h"
#include "chrome/browser/ui/app_list/search/ranking/types.h"
#include "chrome/browser/ui/app_list/search/search_controller.h"

class Profile;

namespace app_list {

// Returns the absolute path of the directory rankers should serialize their
// state into.
base::FilePath RankerStateDirectory(Profile* profile);

// Given a search result type, returns the category it should be placed in.
Category ResultTypeToCategory(ResultType result_type);

// TODO(crbug.com/1199206): Once the UI has support for categories the following
// methods can be removed.

// Given a category, returns a debug string of its name suitable for the interim
// UI.
std::u16string CategoryDebugString(const Category category);

// Deletes a prefix of the form "(...) " from |str| if it exists.
std::u16string RemoveDebugPrefix(std::u16string str);

// Deletes the prefix "(top match) " from |str| if it exists.
std::u16string RemoveTopMatchPrefix(std::u16string str);

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_UTIL_H_
