// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/never_event_storage_validator.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace feature_engagement {

namespace {

class NeverEventStorageValidatorTest : public ::testing::Test {
 public:
  NeverEventStorageValidatorTest() = default;

  NeverEventStorageValidatorTest(const NeverEventStorageValidatorTest&) =
      delete;
  NeverEventStorageValidatorTest& operator=(
      const NeverEventStorageValidatorTest&) = delete;

 protected:
  NeverEventStorageValidator validator_;
};

}  // namespace

TEST_F(NeverEventStorageValidatorTest, ShouldNeverKeep) {
  EXPECT_FALSE(validator_.ShouldStore("dummy event"));
}

TEST_F(NeverEventStorageValidatorTest, ShouldNeverStore) {
  EXPECT_FALSE(validator_.ShouldKeep("dummy event", 99, 100));
  EXPECT_FALSE(validator_.ShouldKeep("dummy event", 100, 100));
  EXPECT_FALSE(validator_.ShouldKeep("dummy event", 101, 100));
}

}  // namespace feature_engagement
