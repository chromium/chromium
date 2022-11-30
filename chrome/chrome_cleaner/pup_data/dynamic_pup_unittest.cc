// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/pup_data/dynamic_pup.h"

#include <string.h>
#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {

namespace {

const UwSId kTestUwSOneId = 26824;  // Chosen at random.
const char kTestUwSOneName[] = "Observed/TestUwSOne";

const UwSId kTestUwSTwoId = 28980;
const char kTestUwSTwoName[] = "Removed/TestUwSTwo";

}  // namespace

TEST(DynamicPUPTest, OnePUP) {
  DynamicPUP pupOne(
      kTestUwSOneName, kTestUwSOneId, PUPData::FLAGS_STATE_CONFIRMED_UWS);

  EXPECT_EQ(pupOne.signature().id, kTestUwSOneId);
  EXPECT_STREQ(pupOne.signature().name, kTestUwSOneName);
  EXPECT_EQ(pupOne.signature().flags, PUPData::FLAGS_STATE_CONFIRMED_UWS);
  EXPECT_NE(pupOne.signature().disk_footprints, nullptr);
  EXPECT_NE(pupOne.signature().registry_footprints, nullptr);
  EXPECT_NE(pupOne.signature().custom_matchers, nullptr);
}

TEST(DynamicPUPTest, MultiPUPs) {
  DynamicPUP pupOne(
      kTestUwSOneName, kTestUwSOneId, PUPData::FLAGS_STATE_CONFIRMED_UWS);
  DynamicPUP pupTwo(kTestUwSTwoName, kTestUwSTwoId, PUPData::FLAGS_NONE);

  EXPECT_EQ(pupOne.signature().id, kTestUwSOneId);
  EXPECT_STREQ(pupOne.signature().name, kTestUwSOneName);
  EXPECT_EQ(pupOne.signature().flags, PUPData::FLAGS_STATE_CONFIRMED_UWS);
  EXPECT_NE(pupOne.signature().disk_footprints, nullptr);
  EXPECT_NE(pupOne.signature().registry_footprints, nullptr);
  EXPECT_NE(pupOne.signature().custom_matchers, nullptr);

  EXPECT_EQ(pupTwo.signature().id, kTestUwSTwoId);
  EXPECT_STREQ(pupTwo.signature().name, kTestUwSTwoName);
  EXPECT_EQ(pupTwo.signature().flags, PUPData::FLAGS_NONE);
  EXPECT_NE(pupTwo.signature().disk_footprints, nullptr);
  EXPECT_NE(pupTwo.signature().registry_footprints, nullptr);
  EXPECT_NE(pupTwo.signature().custom_matchers, nullptr);
}

TEST(DynamicPUPTest, NameGoesOutOfScope) {
  constexpr char kTemporaryName[] = "Temporary name";

  char* temp_name = strdup(kTemporaryName);
  DynamicPUP pup(temp_name, kTestUwSOneId, PUPData::FLAGS_NONE);

  // Overwrite the contents of temp_name. The pup signature should not reflect
  // this change.
  memset(temp_name, 'x', strlen(temp_name));
  EXPECT_STREQ(pup.signature().name, kTemporaryName);

  // The name should have been copied into the pup signature so it will still
  // be valid after freeing temp_name.
  free(temp_name);
  temp_name = nullptr;
  EXPECT_STREQ(pup.signature().name, kTemporaryName);
}

}  // namespace chrome_cleaner
