// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/core/common/crash_key.h"

#include "base/debug/crash_logging.h"
#include "base/debug/stack_trace.h"
#include "base/stl_util.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crash_reporter {
namespace {

class CrashKeyStringTest : public testing::Test {
 public:
  void SetUp() override { InitializeCrashKeysForTesting(); }

  void TearDown() override { ResetCrashKeysForTesting(); }
};

TEST_F(CrashKeyStringTest, ScopedCrashKeyString) {
  static CrashKeyString<32> key("test-scope");

  EXPECT_FALSE(key.is_set());

  {
    ScopedCrashKeyString scoper(&key, "value");
    EXPECT_TRUE(key.is_set());
  }

  EXPECT_FALSE(key.is_set());
}

TEST_F(CrashKeyStringTest, FormatStackTrace) {
  const uintptr_t addresses[] = {
      0x0badbeef, 0x77778888, 0xabc, 0x000ddeeff, 0x12345678,
  };
  base::debug::StackTrace trace(reinterpret_cast<const void* const*>(addresses),
                                base::size(addresses));

  std::string too_small = internal::FormatStackTrace(trace, 3);
  EXPECT_EQ(0u, too_small.size());

  std::string one_value = internal::FormatStackTrace(trace, 16);
  EXPECT_EQ("0xbadbeef", one_value);

  std::string three_values = internal::FormatStackTrace(trace, 30);
  EXPECT_EQ("0xbadbeef 0x77778888 0xabc", three_values);

  std::string all_values = internal::FormatStackTrace(trace, 128);
  EXPECT_EQ("0xbadbeef 0x77778888 0xabc 0xddeeff 0x12345678", all_values);
}

#if defined(ARCH_CPU_64_BITS)
TEST_F(CrashKeyStringTest, FormatStackTrace64) {
  const uintptr_t addresses[] = {
      0xbaaaabaaaaba, 0x1000000000000000,
  };
  base::debug::StackTrace trace(reinterpret_cast<const void* const*>(addresses),
                                base::size(addresses));

  std::string too_small = internal::FormatStackTrace(trace, 8);
  EXPECT_EQ(0u, too_small.size());

  std::string one_value = internal::FormatStackTrace(trace, 20);
  EXPECT_EQ("0xbaaaabaaaaba", one_value);

  std::string all_values = internal::FormatStackTrace(trace, 35);
  EXPECT_EQ("0xbaaaabaaaaba 0x1000000000000000", all_values);
}
#endif

// In certain build configurations, StackTrace will produce an
// empty result, which will cause the test to fail.
#if !defined(OFFICIAL_BUILD) && !defined(NO_UNWIND_TABLES)
TEST_F(CrashKeyStringTest, SetStackTrace) {
  static CrashKeyString<1024> key("test-trace");

  EXPECT_FALSE(key.is_set());

  SetCrashKeyStringToStackTrace(&key, base::debug::StackTrace());

  EXPECT_TRUE(key.is_set());
}
#endif

TEST_F(CrashKeyStringTest, BaseSupport) {
  static base::debug::CrashKeyString* crash_key =
      base::debug::AllocateCrashKeyString("base-support",
                                          base::debug::CrashKeySize::Size64);

  EXPECT_TRUE(crash_key);

  base::debug::SetCrashKeyString(crash_key, "this is a test");

  base::debug::ClearCrashKeyString(crash_key);

  base::debug::SetCrashKeyString(crash_key, std::string(128, 'b'));
  base::debug::SetCrashKeyString(crash_key, std::string(64, 'a'));
}

TEST_F(CrashKeyStringTest, CArrayInitializer) {
  static CrashKeyString<8> keys[] = {
      {"test-1", CrashKeyString<8>::Tag::kArray},
      {"test-2", CrashKeyString<8>::Tag::kArray},
      {"test-3", CrashKeyString<8>::Tag::kArray},
  };

  EXPECT_FALSE(keys[0].is_set());
  EXPECT_FALSE(keys[1].is_set());
  EXPECT_FALSE(keys[2].is_set());

  keys[1].Set("test");

  EXPECT_FALSE(keys[0].is_set());
  EXPECT_TRUE(keys[1].is_set());
  EXPECT_FALSE(keys[2].is_set());
}

}  // namespace
}  // namespace crash_reporter
