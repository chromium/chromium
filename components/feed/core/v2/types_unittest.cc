// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/types.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {
namespace {

std::string ToJSON(base::ValueView value) {
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

  const base::Value::Dict serialized_dict = PersistentMetricsDataToDict(data);
  const PersistentMetricsData deserialized_dict =
      PersistentMetricsDataFromDict(serialized_dict);

  EXPECT_EQ(R"({
   "day_start": "11644473600000000",
   "time_spent_in_feed": "7200000000"
}
)",
            ToJSON(serialized_dict));
  EXPECT_EQ(data.accumulated_time_spent_in_feed,
            deserialized_dict.accumulated_time_spent_in_feed);
  EXPECT_EQ(data.current_day_start, deserialized_dict.current_day_start);
}

TEST(Types, ToContentRevision) {
  const ContentRevision cr = ContentRevision::Generator().GenerateNextId();

  EXPECT_EQ("c/1", ToString(cr));
  EXPECT_EQ(cr, ToContentRevision(ToString(cr)));
  EXPECT_EQ(ContentRevision(), ToContentRevision("2"));
  EXPECT_EQ(ContentRevision(), ToContentRevision("c"));
  EXPECT_EQ(ContentRevision(), ToContentRevision("c/"));
}

TEST(Types, ContentHashSet) {
  feedstore::StreamContentHashList g1;
  g1.add_hashes(3);
  g1.add_hashes(1);
  feedstore::StreamContentHashList g2;
  g2.add_hashes(2);
  ContentHashSet v123(std::vector<feedstore::StreamContentHashList>{g1, g2});
  feedstore::StreamContentHashList h1;
  h1.add_hashes(4);
  feedstore::StreamContentHashList h2;
  h2.add_hashes(1);
  h2.add_hashes(3);
  feedstore::StreamContentHashList h3;
  h3.add_hashes(2);
  ContentHashSet v1234(
      std::vector<feedstore::StreamContentHashList>{h1, h2, h3});

  EXPECT_TRUE(v1234.ContainsAllOf(v123));
  EXPECT_FALSE(v123.ContainsAllOf(v1234));
  EXPECT_TRUE(v1234.Contains(4));
  EXPECT_FALSE(v123.Contains(4));
  EXPECT_FALSE(v123.IsEmpty());
  EXPECT_TRUE(ContentHashSet().IsEmpty());
  /*std::vector<uint32_t> actual_values(
      v1234.values().begin(), v1234.values().end());
  std::vector<uint32_t> expected_values({1, 2, 3, 4});
  EXPECT_EQ(expected_values, actual_values);*/
}

}  // namespace feed
