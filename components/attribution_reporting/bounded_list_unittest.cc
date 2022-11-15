// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/bounded_list.h"

#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace attribution_reporting {

TEST(BoundedListTest, Create) {
  const struct {
    std::vector<int> vec;
    bool expected;
  } kTestCases[] = {
      {{}, true},
      {{1, 2}, true},
      {{1, 2, 3}, false},
  };

  for (const auto& test_case : kTestCases) {
    auto actual = BoundedList<int, 2>::Create(test_case.vec);
    EXPECT_EQ(test_case.expected, actual.has_value()) << test_case.vec.size();
    if (test_case.expected)
      EXPECT_EQ(actual->vec(), test_case.vec) << test_case.vec.size();
  }
}

}  // namespace attribution_reporting
