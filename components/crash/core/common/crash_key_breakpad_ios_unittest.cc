// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/core/common/crash_key.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "third_party/breakpad/breakpad/src/common/simple_string_dictionary.h"

namespace crash_reporter {

namespace {

// The maximum length of a single Breakpad value. Determined by reading the
// documentation for Breakpad's LongStringDictionary.
const size_t kMaxValueSize =
    10 * (google_breakpad::SimpleStringDictionary::value_size - 1);

}  // namespace

class CrashKeyBreakpadIOSTest : public PlatformTest {
 public:
  void SetUp() override { InitializeCrashKeysForTesting(); }

  void TearDown() override { ResetCrashKeysForTesting(); }
};

TEST_F(CrashKeyBreakpadIOSTest, SetClearSingle) {
  static CrashKeyStringBreakpad<32> key("test-key");
  EXPECT_FALSE(key.is_set());

  key.Set("value");
  EXPECT_TRUE(key.is_set());
  EXPECT_EQ("value", GetCrashKeyValue("test-key"));

  key.Set("value 2");
  EXPECT_TRUE(key.is_set());
  EXPECT_EQ("value 2", GetCrashKeyValue("test-key"));

  key.Clear();
  EXPECT_FALSE(key.is_set());
  EXPECT_EQ("", GetCrashKeyValue("test-key"));
}

TEST_F(CrashKeyBreakpadIOSTest, SetMaxLengthValue) {
  static CrashKeyStringBreakpad<2550> key("test-key");

  key.Set(std::string(kMaxValueSize, 'A'));
  EXPECT_EQ(kMaxValueSize, GetCrashKeyValue("test-key").length());
}

TEST_F(CrashKeyBreakpadIOSTest, SetOverflowValue) {
  static CrashKeyStringBreakpad<4000> key("test-key");

  key.Set(std::string(kMaxValueSize + 100, 'A'));
  EXPECT_EQ(kMaxValueSize, GetCrashKeyValue("test-key").length());
}

}  // namespace crash_reporter
