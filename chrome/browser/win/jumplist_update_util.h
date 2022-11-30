// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_JUMPLIST_UPDATE_UTIL_H_
#define CHROME_BROWSER_WIN_JUMPLIST_UPDATE_UTIL_H_

#include "chrome/browser/win/jumplist_updater.h"
#include "components/history/core/browser/history_types.h"

// Checks if the urls stored in |items| are unchanged compared to the first
// |max_item_count| urls in |urls|.
bool MostVisitedItemsUnchanged(const ShellLinkItemList& items,
                               const history::MostVisitedURLList& urls,
                               size_t max_item_count);

#endif  // CHROME_BROWSER_WIN_JUMPLIST_UPDATE_UTIL_H_
