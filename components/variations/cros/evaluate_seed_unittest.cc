// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/cros/evaluate_seed.h"

#include "components/variations/client_filterable_state.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations::evaluate_seed {

TEST(VariationsCrosEvaluateSeed, GetClientFilterable_Enrolled) {
  base::CommandLine command_line({"evaluate_seed", "--enterprise-enrolled"});
  ClientFilterableState state = GetClientFilterableState(&command_line);
  EXPECT_TRUE(state.IsEnterprise());
}

TEST(VariationsCrosEvaluateSeed, GetClientFilterable_NotEnrolled) {
  base::CommandLine command_line({"evaluate_seed"});
  ClientFilterableState state = GetClientFilterableState(&command_line);
  EXPECT_FALSE(state.IsEnterprise());
}

// Should ignore data if flag is off.
TEST(VariationsCrosEvaluateSeed, GetSafeSeedData_Off) {
  std::string text("some text");
  FILE* stream = fmemopen(text.data(), text.size(), "r");
  ASSERT_NE(stream, nullptr);

  base::CommandLine command_line({"evaluate_seed"});
  auto data = GetSafeSeedData(&command_line, stream);
  ASSERT_TRUE(data.has_value());
  EXPECT_FALSE(data.value().use_safe_seed);
  EXPECT_EQ(data.value().seed_data, "");
}

// Should return specified data via stream if flag is on.
TEST(VariationsCrosEvaluateSeed, GetSafeSeedData_On) {
  std::string text("some text");
  FILE* stream = fmemopen(text.data(), text.size(), "r");
  ASSERT_NE(stream, nullptr);

  base::CommandLine command_line({"evaluate_seed", "--use-safe-seed"});
  auto data = GetSafeSeedData(&command_line, stream);
  ASSERT_TRUE(data.has_value());
  EXPECT_TRUE(data.value().use_safe_seed);
  EXPECT_EQ(data.value().seed_data, text);
}

// Should not attempt to read stream if flag is not on.
TEST(VariationsCrosEvaluateSeed, GetSafeSeedData_Off_FailRead) {
  std::string text("some text");
  FILE* stream = fmemopen(text.data(), text.size(), "w");
  ASSERT_NE(stream, nullptr);

  base::CommandLine command_line({"evaluate_seed"});
  auto data = GetSafeSeedData(&command_line, stream);
  ASSERT_TRUE(data.has_value());
  EXPECT_FALSE(data.value().use_safe_seed);
  EXPECT_EQ(data.value().seed_data, "");
}

// If flag is on and reading fails, should return nullopt.
TEST(VariationsCrosEvaluateSeed, GetSafeSeedData_On_FailRead) {
  std::string text("some text");
  FILE* stream = fmemopen(text.data(), text.size(), "w");
  ASSERT_NE(stream, nullptr);

  base::CommandLine command_line({"evaluate_seed", "--use-safe-seed"});
  auto data = GetSafeSeedData(&command_line, stream);
  EXPECT_FALSE(data.has_value());
}

// If flag is on and reading fails, should return nullopt.
TEST(VariationsCrosEvaluateSeed, GetSafeSeedData_On_FailRead_Null) {
  base::CommandLine command_line({"evaluate_seed", "--use-safe-seed"});
  auto data = GetSafeSeedData(&command_line, nullptr);
  EXPECT_FALSE(data.has_value());
}

TEST(VariationsCrosEvaluateSeed, Main_NoFlag) {
  base::CommandLine command_line({"evaluate_seed"});
  EXPECT_EQ(0, EvaluateSeedMain(&command_line, nullptr));
}

TEST(VariationsCrosEvaluateSeed, Main_NoStdin) {
  base::CommandLine command_line({"evaluate_seed", "--use-safe-seed"});
  EXPECT_EQ(1, EvaluateSeedMain(&command_line, nullptr));
}

}  // namespace variations::evaluate_seed
