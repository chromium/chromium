// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/types.h"
#include "base/json/json_writer.h"
#include "base/strings/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {
namespace {

std::string ToJSON(const base::Value& value) {
  std::string json;
  CHECK(base::JSONWriter::WriteWithOptions(
      value, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json));
  // Don't use \r\n on windows.
  base::RemoveChars(json, "\r", &json);
  return json;
}

}  // namespace

TEST(PersistentMetricsData, SerializesAndDeserializes) {
  PersistentMetricsData data;
  data.accumulated_time_spent_in_feed = base::Hours(2);
  data.current_day_start = base::Time::UnixEpoch();

  const base::Value serialized_value = PersistentMetricsDataToValue(data);
  const PersistentMetricsData deserialized_value =
      PersistentMetricsDataFromValue(serialized_value);

  EXPECT_EQ(R"({
   "day_start": "11644473600000000",
   "time_spent_in_feed": "7200000000"
}
)",
            ToJSON(serialized_value));
  EXPECT_EQ(data.accumulated_time_spent_in_feed,
            deserialized_value.accumulated_time_spent_in_feed);
  EXPECT_EQ(data.current_day_start, deserialized_value.current_day_start);
}

TEST(Types, ToContentRevision) {
  const ContentRevision cr = ContentRevision::Generator().GenerateNextId();

  EXPECT_EQ("c/1", ToString(cr));
  EXPECT_EQ(cr, ToContentRevision(ToString(cr)));
  EXPECT_EQ(ContentRevision(), ToContentRevision("2"));
  EXPECT_EQ(ContentRevision(), ToContentRevision("c"));
  EXPECT_EQ(ContentRevision(), ToContentRevision("c/"));
}

TEST(Types, ContentIdSet) {
  ContentIdSet v123(std::vector<int64_t>{1, 2, 3});
  ContentIdSet v1234(std::vector<int64_t>{1, 2, 3, 4});

  EXPECT_TRUE(v1234.ContainsAllOf(v123));
  EXPECT_FALSE(v123.ContainsAllOf(v1234));
  EXPECT_FALSE(v123.IsEmpty());
  EXPECT_TRUE(ContentIdSet().IsEmpty());
}

}  // namespace feed
