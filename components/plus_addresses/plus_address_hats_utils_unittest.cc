// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/plus_address_hats_utils.h"

#include <map>
#include <string>

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/plus_addresses/plus_address_prefs.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace plus_addresses::hats {

namespace {

using testing::Pair;
using testing::UnorderedElementsAre;

constexpr base::TimeDelta kDuration = base::Seconds(100);
}  // namespace

class PlusAddressHatsUtilsTest : public testing::Test {
 public:
  PlusAddressHatsUtilsTest() {
    pref_service_.registry()->RegisterTimePref(
        prefs::kFirstPlusAddressCreationTime, base::Time());
    pref_service_.registry()->RegisterTimePref(
        prefs::kLastPlusAddressFillingTime, base::Time());
  }

  base::test::TaskEnvironment& task_environment() { return task_environment_; }

  TestingPrefServiceSimple& prefs() { return pref_service_; }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingPrefServiceSimple pref_service_;
};

TEST_F(PlusAddressHatsUtilsTest, PrefsNotSet) {
  std::map<std::string, std::string> hats_data =
      GetPlusAddressHatsData(&prefs());
  EXPECT_THAT(hats_data,
              UnorderedElementsAre(
                  Pair(kFirstPlusAddressCreationTime, std::string("-1")),
                  Pair(kLastPlusAddressFillingTime, std::string("-1"))));
}

TEST_F(PlusAddressHatsUtilsTest, PrefsSet) {
  prefs().SetTime(prefs::kFirstPlusAddressCreationTime, base::Time::Now());
  prefs().SetTime(prefs::kLastPlusAddressFillingTime, base::Time::Now());

  task_environment().FastForwardBy(kDuration);

  std::map<std::string, std::string> hats_data =
      GetPlusAddressHatsData(&prefs());
  EXPECT_THAT(hats_data,
              UnorderedElementsAre(
                  Pair(kFirstPlusAddressCreationTime, std::string("100")),
                  Pair(kLastPlusAddressFillingTime, std::string("100"))));
}

}  // namespace plus_addresses::hats
