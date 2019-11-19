// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_metrics_recorder.h"

#include <memory>

#include "base/metrics/metrics_hashes.h"
#include "base/test/task_environment.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Contains;
using ::testing::Not;

namespace password_manager {

namespace {

constexpr char kTestUrl[] = "https://www.example.com/";
constexpr ukm::SourceId kTestSourceId = 0x1234;

using UkmEntry = ukm::builders::PageWithPassword;

// Creates a PasswordManagerMetricsRecorder that reports metrics for kTestUrl.
PasswordManagerMetricsRecorder CreateMetricsRecorder() {
  return PasswordManagerMetricsRecorder(kTestSourceId, GURL(kTestUrl));
}

}  // namespace

TEST(PasswordManagerMetricsRecorder, UserModifiedPasswordField) {
  base::test::TaskEnvironment task_environment_;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  {
    PasswordManagerMetricsRecorder recorder(CreateMetricsRecorder());
    recorder.RecordUserModifiedPasswordField();
  }

  const auto& entries =
      test_ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const auto* entry : entries) {
    EXPECT_EQ(kTestSourceId, entry->source_id);
    test_ukm_recorder.ExpectEntryMetric(
        entry, UkmEntry::kUserModifiedPasswordFieldName, 1);
  }
}

TEST(PasswordManagerMetricsRecorder, UserModifiedPasswordFieldMultipleTimes) {
  base::test::TaskEnvironment task_environment_;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  {
    PasswordManagerMetricsRecorder recorder(CreateMetricsRecorder());
    // Multiple calls should not create more than one entry.
    recorder.RecordUserModifiedPasswordField();
    recorder.RecordUserModifiedPasswordField();
  }
  const auto& entries =
      test_ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const auto* entry : entries) {
    EXPECT_EQ(kTestSourceId, entry->source_id);
    test_ukm_recorder.ExpectEntryMetric(
        entry, UkmEntry::kUserModifiedPasswordFieldName, 1);
  }
}

TEST(PasswordManagerMetricsRecorder, UserModifiedPasswordFieldNotCalled) {
  base::test::TaskEnvironment task_environment_;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  { PasswordManagerMetricsRecorder recorder(CreateMetricsRecorder()); }
  const auto& entries =
      test_ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const auto* entry : entries) {
    EXPECT_EQ(kTestSourceId, entry->source_id);
    EXPECT_FALSE(test_ukm_recorder.EntryHasMetric(
        entry, UkmEntry::kUserModifiedPasswordFieldName));
  }
}

}  // namespace password_manager
