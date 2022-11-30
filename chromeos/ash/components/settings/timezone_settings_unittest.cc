// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/ptr_util.h"
#include "chromeos/ash/components/settings/timezone_settings_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace ash {
namespace system {

using icu::TimeZone;
using icu::UnicodeString;

const char* kTimeZones[] = {
    "America/Los_Angeles", "America/Vancouver",   "America/Chicago",
    "America/Winnipeg",    "America/Mexico_City", "America/Buenos_Aires",
    "Asia/Ho_Chi_Minh",    "Asia/Seoul",          "Europe/Athens",
    "Asia/Ulaanbaatar",
};

class KnownTimeZoneTest : public testing::Test {
 public:
  KnownTimeZoneTest() = default;
  ~KnownTimeZoneTest() override = default;

  void SetUp() override {
    for (const char* id : kTimeZones) {
      timezones_.push_back(
          base::WrapUnique(TimeZone::createTimeZone(UnicodeString(id))));
    }
  }

 protected:
  std::vector<std::unique_ptr<TimeZone>> timezones_;
};

TEST_F(KnownTimeZoneTest, IdMatch) {
  static struct {
    const char* id;
    const char* matched;
  } timezone_match_list[] = {
      // Self matches
      {"America/Los_Angeles", "America/Los_Angeles"},
      {"America/Vancouver", "America/Vancouver"},  // Should not be Los_Angeles
      {"America/Winnipeg", "America/Winnipeg"},
      {"Asia/Seoul", "Asia/Seoul"},
      // Canonical ID matches
      {"Canada/Pacific", "America/Vancouver"},
      {"US/Pacific", "America/Los_Angeles"},
      {"US/Central", "America/Chicago"},
      {"Mexico/General", "America/Mexico_City"},
      {"Asia/Ulan_Bator", "Asia/Ulaanbaatar"},
      // Asia/Saigon is canonical, but the list has Asia/Ho_Chi_Minh
      {"Asia/Saigon", "Asia/Ho_Chi_Minh"},
  };

  for (const auto& pair : timezone_match_list) {
    std::unique_ptr<TimeZone> input(
        TimeZone::createTimeZone(UnicodeString(pair.id)));
    std::unique_ptr<TimeZone> expected(
        TimeZone::createTimeZone(UnicodeString(pair.matched)));
    const TimeZone* actual = GetKnownTimezoneOrNull(*input, timezones_);
    EXPECT_NE(nullptr, actual) << "input=" << pair.id;
    if (actual == nullptr)
      continue;
    UnicodeString actual_id;
    actual->getID(actual_id);
    std::string actual_id_str;
    actual_id.toUTF8String(actual_id_str);
    EXPECT_EQ(*expected, *actual) << "input=" << pair.id << ", "
                                  << "expected=" << pair.matched << ", "
                                  << "actual=" << actual_id_str;
  }
}

TEST_F(KnownTimeZoneTest, NoMatch) {
  static const char* no_match_list[] = {
      "Africa/Juba",                // Not in the list
      "Africa/Tripoli",             //  UTC+2 with no DST != Europe/Athens
      "America/Tijuana",            // Historically != America/Los_Angeles
      "Europe/Sofia",               // Historically != Europe/Athens
      "America/Argentina/Cordoba",  // Historically != America/Buenos_Aires
      "Asia/Tokyo",                 // Historically != Asia/Seoul
  };
  for (const char* id : no_match_list) {
    std::unique_ptr<TimeZone> input(
        TimeZone::createTimeZone(UnicodeString(id)));
    EXPECT_EQ(NULL, GetKnownTimezoneOrNull(*input, timezones_))
        << "input=" << id;
  }
}

}  // namespace system
}  // namespace ash
