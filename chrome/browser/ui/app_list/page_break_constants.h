// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_PAGE_BREAK_CONSTANTS_H_
#define CHROME_BROWSER_UI_APP_LIST_PAGE_BREAK_CONSTANTS_H_

#include <string>

namespace app_list {

extern const char kDefaultPageBreak1[];

// List of default page break app list items that are added by default for first
// time users. These items are added in a particular order to in
// default_app_order.cc.
extern const char* const kDefaultPageBreakAppIds[];

extern const size_t kDefaultPageBreakAppIdsLength;

// Returns true if |item_id| is of a default-installed page break item.
bool IsDefaultPageBreakItem(const std::string& item_id);

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_PAGE_BREAK_CONSTANTS_H_
