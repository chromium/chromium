// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/blocked_content/list_item_position.h"
#include "base/check_op.h"

namespace blocked_content {

ListItemPosition GetListItemPositionFromDistance(size_t distance,
                                                 size_t total_size) {
  DCHECK(total_size);
  if (total_size == 1u) {
    DCHECK_EQ(0u, distance);
    return ListItemPosition::kOnlyItem;
  }

  if (distance == 0)
    return ListItemPosition::kFirstItem;

  if (distance == total_size - 1)
    return ListItemPosition::kLastItem;

  return ListItemPosition::kMiddleItem;
}

}  // namespace blocked_content
