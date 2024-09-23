// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/canonical_topic.h"

#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace privacy_sandbox {

namespace {

using Topic = browsing_topics::Topic;

// Constraints around the currently checked in topics and taxonomy. Changes to
// the taxononmy version or number of topics will fail these tests unless these
// are also updated.
constexpr int kAvailableTaxonomyVersion = 1;
constexpr Topic kLowestTopicID = Topic(1);
constexpr Topic kHighestTopicID = Topic(629);
constexpr Topic kNoFirstLevelTopicID = Topic(500);
constexpr Topic kWithMoreChildrenTopicID = Topic(207);
constexpr Topic kWithOneChildTopicID = Topic(250);

}  // namespace

using CanonicalTopicTest = testing::Test;

TEST_F(CanonicalTopicTest, LocalizedRepresentation) {
  // Confirm that topics at the boundaries convert to strings appropriately.
  base::HistogramTester histogram_tester;
  CanonicalTopic first_topic(kLowestTopicID, kAvailableTaxonomyVersion);
  // The highest topic ID is actually part of a later taxonomy version, but
  // CanonicalTopic no longer uses the version.
  CanonicalTopic last_topic(kHighestTopicID, kAvailableTaxonomyVersion);
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_1),
            first_topic.GetLocalizedRepresentation());
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V2_TOPIC_ID_629),
            last_topic.GetLocalizedRepresentation());
}

TEST_F(CanonicalTopicTest, InvalidTopicIdLocalized) {
  // Confirm that an attempt to localize an invalid Topic ID returns the correct
  // error string and logs to UMA.
  base::HistogramTester histogram_tester;
  CanonicalTopic too_low_id(Topic(kLowestTopicID.value() - 1),
                            kAvailableTaxonomyVersion);
  CanonicalTopic negative_id(Topic(-1), kAvailableTaxonomyVersion);
  CanonicalTopic too_high_id(Topic(kHighestTopicID.value() + 1),
                             kAvailableTaxonomyVersion);

  std::vector<CanonicalTopic> test_bad_topics = {too_low_id, negative_id,
                                                 too_high_id};

  for (const auto& topic : test_bad_topics) {
    EXPECT_EQ(
        l10n_util::GetStringUTF16(IDS_PRIVACY_SANDBOX_TOPICS_INVALID_TOPIC),
        topic.GetLocalizedRepresentation());
  }
}

TEST_F(CanonicalTopicTest, ValueConversion) {
  // Confirm that conversion to and from base::Value forms work correctly.
  CanonicalTopic test_topic(kLowestTopicID, kAvailableTaxonomyVersion);

  auto topic_value = test_topic.ToValue();

  auto converted_topic = CanonicalTopic::FromValue(topic_value);
  EXPECT_TRUE(converted_topic);
  EXPECT_EQ(test_topic, *converted_topic);

  base::Value::Dict invalid_value;
  invalid_value.Set("unrelated", "unrelated");
  converted_topic =
      CanonicalTopic::FromValue(base::Value(std::move(invalid_value)));
  EXPECT_FALSE(converted_topic);
}

TEST_F(CanonicalTopicTest, LocalizedDescription) {
  CanonicalTopic empty_topic(kNoFirstLevelTopicID, kAvailableTaxonomyVersion);
  CanonicalTopic topic_with_one_child(kWithOneChildTopicID,
                                      kAvailableTaxonomyVersion);
  CanonicalTopic topic_with_more_children(kWithMoreChildrenTopicID,
                                          kAvailableTaxonomyVersion);

  EXPECT_EQ(u"", empty_topic.GetLocalizedDescription());
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_253),
            topic_with_one_child.GetLocalizedDescription());
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_SETTINGS_TOPICS_PAGE_FIRST_LEVEL_TOPIC_DESCRIPTOR,
                l10n_util::GetStringUTF16(
                    IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V2_TOPIC_ID_482),
                l10n_util::GetStringUTF16(
                    IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V2_TOPIC_ID_519)),
            topic_with_more_children.GetLocalizedDescription());
}

}  // namespace privacy_sandbox
