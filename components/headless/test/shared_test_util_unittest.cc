// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/headless/test/shared_test_util.h"

#include "base/files/file_path.h"
#include "components/headless/test/test_meta_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace headless {
namespace {

#define FP(path) base::FilePath(FILE_PATH_LITERAL(path))

TEST(HeadlessTestSharedTestUtil, IsSharedTest) {
  static_assert(!IsSharedTestScript("sanity/test-script.js"));
  static_assert(IsSharedTestScript("shared/test-script.js"));
}

class HeadlessTestExpectationsTest
    : public ::testing::TestWithParam<HeadlessType> {
 public:
  base::FilePath GetTestExpectationFilePath(
      const TestMetaInfo& test_meta_info) {
    return headless::GetTestExpectationFilePath(
        FP("shared/script-path.js"), test_meta_info, GetHeadlessType());
  }

  HeadlessType GetHeadlessType() { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    HeadlessTestExpectationsTest,
    ::testing::Values(HeadlessType::kUnspecified,
                      HeadlessType::kHeadlessMode,
                      HeadlessType::kHeadlessShell));

TEST_P(HeadlessTestExpectationsTest, NormalTestExpectations) {
  EXPECT_EQ(GetTestExpectationFilePath(TestMetaInfo()),
            FP("shared/script-path-expected.txt"));
}

TEST_P(HeadlessTestExpectationsTest, ForkedHeadlessModeTestExpectations) {
  TestMetaInfo test_meta_info;
  test_meta_info.fork_headless_mode_expectations = true;
  base::FilePath path = GetTestExpectationFilePath(test_meta_info);

  switch (GetHeadlessType()) {
    case HeadlessType::kUnspecified:
      EXPECT_EQ(path, FP("shared/script-path-expected.txt"));
      break;
    case HeadlessType::kHeadlessMode:
      EXPECT_EQ(path, FP("shared/script-path-headless-mode-expected.txt"));
      break;
    case HeadlessType::kHeadlessShell:
      EXPECT_EQ(path, FP("shared/script-path-expected.txt"));
      break;
  }
}

TEST_P(HeadlessTestExpectationsTest, ForkedHeadlessShellTestExpectations) {
  TestMetaInfo test_meta_info;
  test_meta_info.fork_headless_shell_expectations = true;
  base::FilePath path = GetTestExpectationFilePath(test_meta_info);

  switch (GetHeadlessType()) {
    case HeadlessType::kUnspecified:
      EXPECT_EQ(path, FP("shared/script-path-expected.txt"));
      break;
    case HeadlessType::kHeadlessMode:
      EXPECT_EQ(path, FP("shared/script-path-expected.txt"));
      break;
    case HeadlessType::kHeadlessShell:
      EXPECT_EQ(path, FP("shared/script-path-headless-shell-expected.txt"));
      break;
  }
}

}  // namespace
}  // namespace headless
