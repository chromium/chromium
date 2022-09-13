// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/execution_context_priority/execution_context_priority.h"

#include "components/performance_manager/test_support/voting.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {
namespace execution_context_priority {

namespace {

static const char kReason1[] = "reason1";
static const char kReason2[] = "reason2";
static const char kReason3[] = "reason1";  // Equal to kReason1 on purpose!

}  // namespace

TEST(ExecutionContextPriorityTest, ReasonCompare) {
  // Comparison with nullptr.
  EXPECT_GT(0, ReasonCompare(nullptr, kReason1));
  EXPECT_EQ(0, ReasonCompare(nullptr, nullptr));
  EXPECT_LT(0, ReasonCompare(kReason1, nullptr));

  // Comparisons where the addresses and string content are different.
  EXPECT_GT(0, ReasonCompare(kReason1, kReason2));
  EXPECT_LT(0, ReasonCompare(kReason2, kReason1));

  // Comparison with identical addresses.
  EXPECT_EQ(0, ReasonCompare(kReason1, kReason1));

  // Comparison where the addresses are different, but string content is the
  // same.
  EXPECT_EQ(0, ReasonCompare(kReason1, kReason3));
}

TEST(ExecutionContextPriorityTest, PriorityAndReason) {
  // Default constructor
  PriorityAndReason par1;
  EXPECT_EQ(base::TaskPriority::LOWEST, par1.priority());
  EXPECT_EQ(nullptr, par1.reason());

  // Explicit initialization.
  PriorityAndReason par2(base::TaskPriority::HIGHEST, kReason1);
  EXPECT_EQ(base::TaskPriority::HIGHEST, par2.priority());
  EXPECT_EQ(kReason1, par2.reason());

  // Identical comparison.
  EXPECT_TRUE(par1 == par1);
  EXPECT_FALSE(par1 != par1);
  EXPECT_TRUE(par1 <= par1);
  EXPECT_TRUE(par1 >= par1);
  EXPECT_FALSE(par1 < par1);
  EXPECT_FALSE(par1 > par1);

  // Comparison with distinct priorities.
  EXPECT_FALSE(par1 == par2);
  EXPECT_TRUE(par1 != par2);
  EXPECT_TRUE(par1 <= par2);
  EXPECT_FALSE(par1 >= par2);
  EXPECT_TRUE(par1 < par2);
  EXPECT_FALSE(par1 > par2);

  // Comparison with identical priorities and reasons strings, but at different
  // locations.
  PriorityAndReason par3(base::TaskPriority::HIGHEST, kReason3);
  EXPECT_EQ(base::TaskPriority::HIGHEST, par3.priority());
  EXPECT_EQ(kReason3, par3.reason());
  EXPECT_TRUE(par2 == par3);
  EXPECT_FALSE(par2 != par3);
  EXPECT_TRUE(par2 <= par3);
  EXPECT_TRUE(par2 >= par3);
  EXPECT_FALSE(par2 < par3);
  EXPECT_FALSE(par2 > par3);

  // Comparison with identical priorities, and different reason strings.
  PriorityAndReason par4(base::TaskPriority::LOWEST, kReason2);
  EXPECT_FALSE(par1 == par4);
  EXPECT_TRUE(par1 != par4);
  EXPECT_TRUE(par1 <= par4);
  EXPECT_FALSE(par1 >= par4);
  EXPECT_TRUE(par1 < par4);
  EXPECT_FALSE(par1 > par4);

  // Copy constructor.
  PriorityAndReason par5(par3);
  EXPECT_EQ(base::TaskPriority::HIGHEST, par5.priority());
  EXPECT_EQ(kReason3, par5.reason());

  // Assignment.
  par1 = par3;
  EXPECT_EQ(base::TaskPriority::HIGHEST, par1.priority());
  EXPECT_EQ(kReason3, par1.reason());
}

}  // namespace execution_context_priority
}  // namespace performance_manager
