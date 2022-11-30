// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/test/mock_helper.h"

#include "testing/gtest/include/gtest/gtest-spi.h"

namespace {
class TestingMock {
 public:
  MOCK_METHOD0(Test, void(void));
};

TEST(ExpectMockFailureTest, FailsWhenNoMock) {
  EXPECT_NONFATAL_FAILURE({ EXPECT_MOCK_FAILURE({ ; }); }, "");
}

TEST(ExpectMockFailureTest, FailsWhenMockSucceeds) {
  EXPECT_NONFATAL_FAILURE(
      {
        EXPECT_MOCK_FAILURE({
          ::testing::NiceMock<TestingMock> t1;
          EXPECT_CALL(t1, Test());

          t1.Test();
        });
      },
      "");
}

TEST(ExpectMockFailureTest, PassesWhenMockFailsForMissing) {
  EXPECT_MOCK_FAILURE({
    ::testing::NiceMock<TestingMock> t1;
    EXPECT_CALL(t1, Test());
  });
}

TEST(ExpectMockFailureTest, PassesWhenMockFailsForUnexpected) {
  EXPECT_MOCK_FAILURE({
    ::testing::StrictMock<TestingMock> t1;
    t1.Test();
  });
}

}  // namespace
