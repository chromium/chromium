// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/vector_icons/vector_icons_unittest.h"

#include "base/compiler_specific.h"
#include "components/vector_icons/vector_icons.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/vector_icon_utils.h"

namespace vector_icons {

class VectorIconsTest : public ::testing::Test {
 public:
  void CheckThatParsedElementsMatch(const std::string& s,
                                    const gfx::VectorIcon& icon) {
    std::vector<std::vector<gfx::PathElement>> path_elements;
    gfx::ParsePathElements(s, path_elements);
    EXPECT_EQ(icon.reps.size(), path_elements.size());
    for (size_t i = 0; i < path_elements.size(); ++i) {
      EXPECT_EQ(icon.reps[i].path.size(), path_elements[i].size());
      for (size_t j = 0; j < path_elements[i].size(); ++j) {
        EXPECT_EQ(
            0, UNSAFE_TODO(memcmp(&path_elements[i][j], &icon.reps[i].path[j],
                                  sizeof(gfx::PathElement))));
      }
    }
  }
};

VECTOR_ICON_TEST_CASES

}  // namespace vector_icons
