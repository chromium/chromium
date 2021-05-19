// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_UTIL_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_UTIL_H_

#include <string>

namespace app_list {

// TODO(crbug.com/1199206): Once the UI has support for categories this can be
// removed.

// Deletes a prefix of the form "(...) " from |str| if it exists.
std::u16string RemoveDebugPrefix(std::u16string str);

// Deletes the prefix "(top match) " from |str| if it exists.
std::u16string RemoveTopMatchPrefix(std::u16string str);

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_RANKING_UTIL_H_
