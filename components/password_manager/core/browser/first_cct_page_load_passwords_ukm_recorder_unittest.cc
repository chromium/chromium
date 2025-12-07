// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/first_cct_page_load_passwords_ukm_recorder.h"

#include "base/test/task_environment.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

const ukm::mojom::UkmEntry* GetMetricEntry(
    const ukm::TestUkmRecorder& test_ukm_recorder,
    std::string_view entry) {
  std::vector<raw_ptr<const ukm::mojom::UkmEntry, VectorExperimental>>
      ukm_entries = test_ukm_recorder.GetEntriesByName(entry);
  EXPECT_EQ(1u, ukm_entries.size());
  return ukm_entries[0];
}

}  // namespace

class FirstCctPageLoadPasswordsUkmRecorderTest : public testing::Test {
 public:
  ~FirstCctPageLoadPasswordsUkmRecorderTest() override = default;

 private:
  // Needed by `TestUkmRecorder`;
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(FirstCctPageLoadPasswordsUkmRecorderTest, RecordsHasNoPwdFormIfNotSet) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  auto first_cct_page_recorder =
      std::make_unique<FirstCctPageLoadPasswordsUkmRecorder>(ukm::SourceId(1));
  first_cct_page_recorder.reset();

  ukm::TestUkmRecorder::ExpectEntryMetric(
      GetMetricEntry(
          test_ukm_recorder,
          ukm::builders::PasswordManager_FirstCCTPageLoad::kEntryName),
      ukm::builders::PasswordManager_FirstCCTPageLoad::kHasPasswordFormName, 0);
}

TEST_F(FirstCctPageLoadPasswordsUkmRecorderTest, RecordsHasPwdFormIfSet) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  auto first_cct_page_recorder =
      std::make_unique<FirstCctPageLoadPasswordsUkmRecorder>(ukm::SourceId(1));
  first_cct_page_recorder->RecordHasPasswordForm();
  first_cct_page_recorder.reset();

  ukm::TestUkmRecorder::ExpectEntryMetric(
      GetMetricEntry(
          test_ukm_recorder,
          ukm::builders::PasswordManager_FirstCCTPageLoad::kEntryName),
      ukm::builders::PasswordManager_FirstCCTPageLoad::kHasPasswordFormName, 1);
}

}  // namespace password_manager
