// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/tasks/prefetch_task_test_base.h"

#include <algorithm>
#include <set>

#include "base/stl_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

// Checks that the PrefetchItemState enum and the |kOrderedPrefetchItemStates|
// are in sync with each other.
TEST(PrefetchTaskTestBaseTest, StateEnumIsFullyRepresentedInOrderedArray) {
  const std::array<PrefetchItemState, 11>& kOrderedPrefetchItemStates =
      PrefetchTaskTestBase::kOrderedPrefetchItemStates;
  size_t element_count = 0;
  // If a new element is added to the enum the switch clause below will cause a
  // build error. When the new element is then added here, the test will fail
  // until it is properly added to |kOrderedPrefetchItemStates|.
  // Note: this code assumes that the minimum assigned value in the enum is 0
  // and that the maximum is correctly represented by the kMaxValue labeled
  // element.
  for (int i = 0; i <= static_cast<int>(PrefetchItemState::kMaxValue); ++i) {
    PrefetchItemState maybe_valid_state = static_cast<PrefetchItemState>(i);
    switch (maybe_valid_state) {
      case PrefetchItemState::NEW_REQUEST:
      case PrefetchItemState::SENT_GENERATE_PAGE_BUNDLE:
      case PrefetchItemState::AWAITING_GCM:
      case PrefetchItemState::RECEIVED_GCM:
      case PrefetchItemState::SENT_GET_OPERATION:
      case PrefetchItemState::RECEIVED_BUNDLE:
      case PrefetchItemState::DOWNLOADING:
      case PrefetchItemState::DOWNLOADED:
      case PrefetchItemState::IMPORTING:
      case PrefetchItemState::FINISHED:
      case PrefetchItemState::ZOMBIE:
        EXPECT_TRUE(
            base::Contains(kOrderedPrefetchItemStates, maybe_valid_state))
            << "Valid state was not found in the array: " << i;
        ++element_count;
        break;
    }
  }
  EXPECT_EQ(kOrderedPrefetchItemStates.size(), element_count);
}

TEST(PrefetchTaskTestBaseTest, CheckOrderedArrayIsSorted) {
  EXPECT_TRUE(
      std::is_sorted(PrefetchTaskTestBase::kOrderedPrefetchItemStates.begin(),
                     PrefetchTaskTestBase::kOrderedPrefetchItemStates.end()));
}

}  // namespace offline_pages
