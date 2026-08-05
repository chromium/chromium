// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/canonical_topic.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace privacy_sandbox {

namespace {


// Constraints around the currently checked in topics and taxonomy. Changes to
// the taxononmy version or number of topics will fail these tests unless these
// are also updated.
constexpr int kAvailableTaxonomyVersion = 1;
constexpr int kLowestTopicID = 1;

}  // namespace

using CanonicalTopicTest = testing::Test;

TEST_F(CanonicalTopicTest, ValueConversion) {
  // Confirm that conversion to and from base::Value forms work correctly.
  CanonicalTopic test_topic(kLowestTopicID, kAvailableTaxonomyVersion);

  auto topic_value = test_topic.ToValue();

  auto converted_topic = CanonicalTopic::FromValue(topic_value);
  EXPECT_TRUE(converted_topic);
  EXPECT_EQ(test_topic, *converted_topic);

  base::DictValue invalid_value;
  invalid_value.Set("unrelated", "unrelated");
  converted_topic =
      CanonicalTopic::FromValue(base::Value(std::move(invalid_value)));
  EXPECT_FALSE(converted_topic);
}

}  // namespace privacy_sandbox
