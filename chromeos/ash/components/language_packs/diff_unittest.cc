// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/language_packs/diff.h"

#include <string>

#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::language_packs {

namespace {

class DiffTest : public testing::Test {};

struct ComputeStringsDiffTestCase {
  std::string test_name;

  std::vector<std::string> current;
  std::vector<std::string> target;

  base::flat_set<std::string> expected_remove;
  base::flat_set<std::string> expected_add;
};

class ComputeStringsDiffTest
    : public DiffTest,
      public testing::WithParamInterface<ComputeStringsDiffTestCase> {};

TEST_P(ComputeStringsDiffTest, Test) {
  const ComputeStringsDiffTestCase& test_case = GetParam();

  const StringsDiff result =
      ComputeStringsDiff(test_case.current, test_case.target);

  EXPECT_EQ(result.add, test_case.expected_add);
  EXPECT_EQ(result.remove, test_case.expected_remove);
}

INSTANTIATE_TEST_SUITE_P(
    ComputeStringsDiffTests,
    ComputeStringsDiffTest,
    testing::ValuesIn<ComputeStringsDiffTestCase>({
        {"AllEmpty", {}, {}, {}, {}},
        {"CurrentEmpty", {}, {"foo", "bar", "baz"}, {}, {"foo", "bar", "baz"}},
        {"TargetEmpty", {"foo", "bar", "baz"}, {}, {"foo", "bar", "baz"}, {}},
        {"CurrentDuplicate", {"foo", "bar", "foo"}, {"bar"}, {"foo"}, {}},
        {"CurrentDuplicateNoop", {"foo", "bar", "foo"}, {"foo", "bar"}, {}, {}},
        {"TargetDuplicate", {"bar"}, {"foo", "bar", "foo"}, {}, {"foo"}},
        {"TargetDuplicateNoop", {"foo", "bar"}, {"foo", "bar", "foo"}, {}, {}},
        {"RemoveAndAddOverlap",
         {"foo", "bar"},
         {"bar", "baz"},
         {"foo"},
         {"baz"}},
        {"RemoveAndAddNoOverlap",
         {"foo", "bar"},
         {"baz", "qux"},
         {"foo", "bar"},
         {"baz", "qux"}},
    }),
    [](const testing::TestParamInfo<ComputeStringsDiffTest::ParamType>& info) {
      return info.param.test_name;
    });

}  // namespace

}  // namespace ash::language_packs
