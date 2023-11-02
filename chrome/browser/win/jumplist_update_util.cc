// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/jumplist_update_util.h"

#include <algorithm>
#include <iterator>

bool MostVisitedItemsUnchanged(const ShellLinkItemList& items,
                               const history::MostVisitedURLList& urls,
                               size_t max_item_count) {
  // If the number of urls going to be displayed doesn't equal to the current
  // one, the most visited items are considered to have changes.
  // Otherwise, check if the current urls stored in |items| equal to the first
  // |max_item_count| urls in |urls| to determine if the most visited items
  // are changed or not.

  if (std::min(urls.size(), max_item_count) != items.size())
    return false;

  return std::equal(std::begin(items), std::end(items), std::begin(urls),
                    [](const auto& item_ptr, const auto& most_visited_url) {
                      return item_ptr->url() == most_visited_url.url.spec();
                    });
}
