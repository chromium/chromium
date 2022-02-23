// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/desks_storage/core/desk_template_util.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace desks_storage {

using DeskTemplateUtilTest = testing::Test;

TEST_F(DeskTemplateUtilTest, AddSequenceNumberForFirstDuplicate) {
  EXPECT_EQ(u"test (1)",
            desk_template_util::AppendDuplicateNumberToDuplicateName(u"test"));
}

TEST_F(DeskTemplateUtilTest, IncrementSequenceNumberOnSubsequentDuplicate) {
  EXPECT_EQ(
      u"test (2)",
      desk_template_util::AppendDuplicateNumberToDuplicateName(u"test (1)"));
  EXPECT_EQ(
      u"test (10)",
      desk_template_util::AppendDuplicateNumberToDuplicateName(u"test (9)"));
  EXPECT_EQ(
      u"test (11)",
      desk_template_util::AppendDuplicateNumberToDuplicateName(u"test (10)"));
  EXPECT_EQ(
      u"test (101)",
      desk_template_util::AppendDuplicateNumberToDuplicateName(u"test (100)"));
}

TEST_F(DeskTemplateUtilTest, OnlyIncrementTheLastSequenceNumber) {
  EXPECT_EQ(u"test (1) (2)",
            desk_template_util::AppendDuplicateNumberToDuplicateName(
                u"test (1) (1)"));
  EXPECT_EQ(u"test (1) (10)",
            desk_template_util::AppendDuplicateNumberToDuplicateName(
                u"test (1) (9)"));
}

}  // namespace desks_storage
