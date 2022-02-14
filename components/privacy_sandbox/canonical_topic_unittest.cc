// Copyright 2022 The Chromium Authors. All rights reserved.
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

// Constraints around the currently checked in topics and taxonomy. Changes to
// the taxononmy version or number of topics will fail these tests unless these
// are also updated.
constexpr int kAvailableTaxononmyVersion = 1;
constexpr int kLowestTopicID = 1;
constexpr int kHighestTopicID = 349;

constexpr char kInvalidTopicLocalizedHistogramName[] =
    "Settings.PrivacySandbox.InvalidTopicIdLocalized";

}  // namespace

using CanonicalTopicTest = testing::Test;

TEST_F(CanonicalTopicTest, LocalizedRepresentation) {
  // Confirm that topics at the boundaries convert to strings appropriately.
  base::HistogramTester histogram_tester;
  CanonicalTopic first_topic(
      kLowestTopicID, privacy_sandbox::CanonicalTopic::AVAILABLE_TAXONOMY);
  CanonicalTopic last_topic(
      kHighestTopicID, privacy_sandbox::CanonicalTopic::AVAILABLE_TAXONOMY);
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_1),
            first_topic.GetLocalizedRepresentation());
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_PRIVACY_SANDBOX_TOPICS_TAXONOMY_V1_TOPIC_ID_349),
            last_topic.GetLocalizedRepresentation());

  // Successful localizations should not result in any metrics being recorded.
  histogram_tester.ExpectTotalCount(kInvalidTopicLocalizedHistogramName, 0);
}

TEST_F(CanonicalTopicTest, InvalidTopicIdLocalized) {
  // Confirm that an attempt to localize an invalid Topic ID returns the correct
  // error string and logs to UMA.
  base::HistogramTester histogram_tester;
  CanonicalTopic too_low_id(kLowestTopicID - 1, kAvailableTaxononmyVersion);
  CanonicalTopic negative_id(-1, kAvailableTaxononmyVersion);
  CanonicalTopic too_high_id(kHighestTopicID + 1, kAvailableTaxononmyVersion);

  std::vector<CanonicalTopic> test_bad_topics = {too_low_id, negative_id,
                                                 too_high_id};

  for (const auto& topic : test_bad_topics) {
    EXPECT_EQ(
        l10n_util::GetStringUTF16(IDS_PRIVACY_SANDBOX_TOPICS_INVALID_TOPIC),
        topic.GetLocalizedRepresentation());
  }

  histogram_tester.ExpectTotalCount(kInvalidTopicLocalizedHistogramName, 3);
  histogram_tester.ExpectBucketCount(kInvalidTopicLocalizedHistogramName,
                                     too_low_id.topic_id(), 1);
  histogram_tester.ExpectBucketCount(kInvalidTopicLocalizedHistogramName,
                                     negative_id.topic_id(), 1);
  histogram_tester.ExpectBucketCount(kInvalidTopicLocalizedHistogramName,
                                     too_high_id.topic_id(), 1);
}

using CanonicalTopicDeathTest = testing::Test;

TEST_F(CanonicalTopicDeathTest, OutOfBoundsDeath) {
  // Confirm that requesting a topics with invalid Taxononmy results in a
  // CHECK failure.
  CanonicalTopic too_low_taxonomy(kLowestTopicID,
                                  kAvailableTaxononmyVersion - 1);
  CanonicalTopic negative_taxonomy(kLowestTopicID, -1);
  CanonicalTopic too_high_taxonomy(kLowestTopicID,
                                   kAvailableTaxononmyVersion + 1);

  std::vector<CanonicalTopic> test_bad_topics = {
      too_low_taxonomy, negative_taxonomy, too_high_taxonomy};

  for (const auto& topic : test_bad_topics)
    EXPECT_CHECK_DEATH(topic.GetLocalizedRepresentation());
}

}  // namespace privacy_sandbox
