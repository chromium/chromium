// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_data_importer_utils.h"

#include <vector>

#include "base/time/time.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

// As TimestampedSameOriginQueue cannot be initialized with primitive types,
// this wrapper is used for testing.
struct IntWrapper {
  int value;
};
bool operator==(IntWrapper x, int y) {
  return x.value == y;
}

}  // anonymous namespace

// TimestampedSameOriginQueue's queue-like functionality works as expected.
TEST(FormDataImporterUtilsTest, TimestampedSameOriginQueue) {
  TimestampedSameOriginQueue<IntWrapper> queue;
  EXPECT_TRUE(queue.empty());
  const url::Origin irrelevant_origin;
  for (int i = 0; i < 4; i++)
    queue.Push({i}, irrelevant_origin);
  EXPECT_EQ(queue.size(), 4u);
  EXPECT_THAT(queue, testing::ElementsAre(3, 2, 1, 0));
  queue.Pop();
  EXPECT_THAT(queue, testing::ElementsAre(3, 2, 1));
  queue.erase(std::next(queue.begin()), queue.end());
  EXPECT_THAT(queue, testing::ElementsAre(3));
  queue.Clear();
  EXPECT_TRUE(queue.empty());
}

// RemoveOutdatedItems clears the queue if the origin doesn't match.
TEST(FormDataImporterUtilsTest, TimestampedSameOriginQueue_DifferentOrigins) {
  TimestampedSameOriginQueue<IntWrapper> queue;
  auto foo_origin = url::Origin::Create(GURL("http://foo.com"));
  queue.Push({0}, foo_origin);
  EXPECT_EQ(queue.Origin(), foo_origin);
  // The TTL or 1 hour is irrelevant here.
  queue.RemoveOutdatedItems(base::Hours(1),
                            url::Origin::Create(GURL("http://bar.com")));
  EXPECT_EQ(queue.Origin(), absl::nullopt);
  EXPECT_TRUE(queue.empty());
}

// RemoveOutdatedItems clears items past their TTL.
TEST(FormDataImporterUtilsTest, TimestampedSameOriginQueue_TTL) {
  TimestampedSameOriginQueue<IntWrapper> queue;
  const url::Origin irrelevant_origin;
  TestAutofillClock test_clock;
  for (int i = 0; i < 4; i++) {
    queue.Push({i}, irrelevant_origin);
    test_clock.Advance(base::Minutes(1));
  }
  // Remove all items older than 2.5 min.
  queue.RemoveOutdatedItems(base::Seconds(150), irrelevant_origin);
  EXPECT_THAT(queue, testing::ElementsAre(3, 2));
}

TEST(FormDataImporterUtilsTest, GetPredictedCountryCode) {
  AutofillProfile us_profile;
  us_profile.SetRawInfo(ADDRESS_HOME_COUNTRY, u"US");
  AutofillProfile empty_profile;
  // Test prioritization: profile > variation service state > app locale
  EXPECT_EQ(GetPredictedCountryCode(us_profile, "DE", "de-AT", nullptr), "US");
  EXPECT_EQ(GetPredictedCountryCode(us_profile, "", "de-AT", nullptr), "US");
  EXPECT_EQ(GetPredictedCountryCode(empty_profile, "DE", "de-AT", nullptr),
            "DE");
  EXPECT_EQ(GetPredictedCountryCode(empty_profile, "", "de-AT", nullptr), "AT");
}

}  // namespace autofill
