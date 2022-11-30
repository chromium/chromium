// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BLOCKED_CONTENT_LIST_ITEM_POSITION_H_
#define COMPONENTS_BLOCKED_CONTENT_LIST_ITEM_POSITION_H_

#include <cstddef>

namespace blocked_content {

// This enum backs a histogram. Make sure you update enums.xml if you make
// any changes.
//
// Identifies an element's position in an ordered list. Used by both the
// framebust and popup UI on desktop platforms to indicate which element was
// clicked.
enum class ListItemPosition : int {
  kOnlyItem = 0,
  kFirstItem = 1,
  kMiddleItem = 2,
  kLastItem = 3,

  // Any new values should go before this one.
  kMaxValue = kLastItem,
};

// Gets the list item position from the given distance/index and the total size
// of the collection. Distance is the measure from the beginning of the
// collection to the given element.
ListItemPosition GetListItemPositionFromDistance(size_t distance,
                                                 size_t total_size);

}  // namespace blocked_content

#endif  // COMPONENTS_BLOCKED_CONTENT_LIST_ITEM_POSITION_H_
