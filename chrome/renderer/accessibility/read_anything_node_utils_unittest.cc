// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/accessibility/read_anything_node_utils.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_node_position.h"

class ReadAnythingNodeUtilsTest : public testing::Test {
 protected:
  ReadAnythingNodeUtilsTest() = default;
};

using testing::ElementsAre;
using testing::IsEmpty;

TEST_F(ReadAnythingNodeUtilsTest,
       IsTextForReadAnything_ReturnsFalseOnNullNode) {
  EXPECT_FALSE(a11y::IsTextForReadAnything(nullptr, false, false));
}
