// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/content_annotator/content_classifier_metrics.h"

#include <cstdint>
#include <string>

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/accessibility_annotator/core/prefs.h"
#include "components/prefs/testing_pref_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace accessibility_annotator {
namespace {

using ::testing::AllOf;
using ::testing::Ge;
using ::testing::Lt;

class ContentClassifierMetricsTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

// Tests that the UKM logging user secret is created and stored in the prefs,
// and that it is updated every 28 days.
TEST_F(ContentClassifierMetricsTest, GetOrCreateUkmLoggingUserSecret) {
  TestingPrefServiceSimple test_pref_service;
  prefs::RegisterProfilePrefs(test_pref_service.registry());

  std::string secret1 = GetOrCreateUkmLoggingUserSecret(&test_pref_service);
  // The secret should not be empty.
  EXPECT_FALSE(secret1.empty());

  // Check that the secret is deterministic.
  std::string secret2 = GetOrCreateUkmLoggingUserSecret(&test_pref_service);
  EXPECT_EQ(secret1, secret2);

  task_environment_.FastForwardBy(base::Days(28) + base::Seconds(1));

  std::string secret3 = GetOrCreateUkmLoggingUserSecret(&test_pref_service);
  EXPECT_NE(secret1, secret3);
}

// Tests that the deterministic noise added to the UKM metric is within the
// expected range and is consistent for the same inputs, but different for
// different inputs.
TEST_F(ContentClassifierMetricsTest, AddDeterministicNoise) {
  uint32_t noisy1 = AddDeterministicNoise(0, "secret1example.com/path1");
  uint32_t noisy1_repeated =
      AddDeterministicNoise(0, "secret1example.com/path1");
  uint32_t noisy2 = AddDeterministicNoise(0, "secret1example.com/path2");
  uint32_t noisy3 = AddDeterministicNoise(0, "secret2example.com/path1");

  EXPECT_EQ(noisy1, noisy1_repeated);
  EXPECT_THAT(noisy1, AllOf(Ge(0u), Lt(16u)));
  EXPECT_THAT(noisy2, AllOf(Ge(0u), Lt(16u)));
  EXPECT_THAT(noisy3, AllOf(Ge(0u), Lt(16u)));
}

// Tests that the semantic classification value score is logged to UKM with
// noise added within the expected range.
TEST_F(ContentClassifierMetricsTest, LogSemanticClassificationValueScore) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  ukm::SourceId source_id = ukm::AssignNewSourceId();
  ukm::builders::AccessibilityAnnotator_ContentAnnotator_ClassifierResults
      ukm_builder(source_id);

  LogSemanticClassificationValueScore(
      0.5, "secret1", GURL("https://example.com/path1"), ukm_builder);
  ukm_builder.Record(ukm::UkmRecorder::Get());

  using UkmEntry =
      ukm::builders::AccessibilityAnnotator_ContentAnnotator_ClassifierResults;
  auto entries = ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  ASSERT_EQ(1u, entries.size());

  const int64_t* metric = ukm_recorder.GetEntryMetric(
      entries[0], UkmEntry::kSemanticClassificationValueScoreName);
  ASSERT_TRUE(metric);

  EXPECT_THAT(*metric, AllOf(Ge(0), Lt(16)));
}

}  // namespace
}  // namespace accessibility_annotator
