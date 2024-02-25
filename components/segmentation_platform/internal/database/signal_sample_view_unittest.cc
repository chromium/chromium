// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/database/signal_sample_view.h"

#include "base/metrics/metrics_hashes.h"
#include "base/test/simple_test_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {

constexpr char kUserAction[] = "UserAction1";
constexpr char kValueHistogram[] = "Value.Histogram";
constexpr char kEnumHistogram[] = "Enum.Histogram";

class SignalSampleViewTest : public testing::Test {
 public:
  SignalSampleViewTest() = default;
  ~SignalSampleViewTest() override = default;

  std::vector<SignalDatabase::DbEntry> GetDatabaseSamples() {
    return std::vector<SignalDatabase::DbEntry>{
        SignalDatabase::DbEntry{.type = proto::SignalType::USER_ACTION,
                                .name_hash = base::HashMetricName(kUserAction),
                                .time = clock_.Now() - base::Hours(100),
                                .value = 0},
        SignalDatabase::DbEntry{.type = proto::SignalType::USER_ACTION,
                                .name_hash = base::HashMetricName(kUserAction),
                                .time = clock_.Now() - base::Hours(89),
                                .value = 0},
        SignalDatabase::DbEntry{.type = proto::SignalType::USER_ACTION,
                                .name_hash = base::HashMetricName(kUserAction),
                                .time = clock_.Now() - base::Hours(64),
                                .value = 0},
        SignalDatabase::DbEntry{.type = proto::SignalType::USER_ACTION,
                                .name_hash = base::HashMetricName(kUserAction),
                                .time = clock_.Now() - base::Hours(55),
                                .value = 1},
        SignalDatabase::DbEntry{.type = proto::SignalType::USER_ACTION,
                                .name_hash = base::HashMetricName(kUserAction),
                                .time = clock_.Now() - base::Hours(40),
                                .value = 2},
        SignalDatabase::DbEntry{.type = proto::SignalType::USER_ACTION,
                                .name_hash = base::HashMetricName(kUserAction),
                                .time = clock_.Now() - base::Hours(20),
                                .value = 3},
        SignalDatabase::DbEntry{.type = proto::SignalType::USER_ACTION,
                                .name_hash = base::HashMetricName(kUserAction),
                                .time = clock_.Now() - base::Hours(10),
                                .value = 1},
        SignalDatabase::DbEntry{.type = proto::SignalType::USER_ACTION,
                                .name_hash = base::HashMetricName(kUserAction),
                                .time = clock_.Now(),
                                .value = 2},
        SignalDatabase::DbEntry{.type = proto::SignalType::USER_ACTION,
                                .name_hash = base::HashMetricName(kUserAction),
                                .time = clock_.Now() + base::Hours(2),
                                .value = 3},
        SignalDatabase::DbEntry{.type = proto::SignalType::USER_ACTION,
                                .name_hash = base::HashMetricName(kUserAction),
                                .time = clock_.Now() + base::Hours(25),
                                .value = 4},

        SignalDatabase::DbEntry{
            .type = proto::SignalType::HISTOGRAM_VALUE,
            .name_hash = base::HashMetricName(kValueHistogram),
            .time = clock_.Now() - base::Hours(50),
            .value = 1},
        SignalDatabase::DbEntry{
            .type = proto::SignalType::HISTOGRAM_VALUE,
            .name_hash = base::HashMetricName(kValueHistogram),
            .time = clock_.Now() - base::Hours(30),
            .value = 2},
        SignalDatabase::DbEntry{
            .type = proto::SignalType::HISTOGRAM_VALUE,
            .name_hash = base::HashMetricName(kValueHistogram),
            .time = clock_.Now() - base::Hours(15),
            .value = 3},
        SignalDatabase::DbEntry{
            .type = proto::SignalType::HISTOGRAM_VALUE,
            .name_hash = base::HashMetricName(kValueHistogram),
            .time = clock_.Now(),
            .value = 4},
        SignalDatabase::DbEntry{
            .type = proto::SignalType::HISTOGRAM_VALUE,
            .name_hash = base::HashMetricName(kValueHistogram),
            .time = clock_.Now() + base::Hours(20),
            .value = 5},

        // Enum histogram can be used as value as well.
        SignalDatabase::DbEntry{
            .type = proto::SignalType::HISTOGRAM_VALUE,
            .name_hash = base::HashMetricName(kEnumHistogram),
            .time = clock_.Now() - base::Hours(3),
            .value = 1},
        SignalDatabase::DbEntry{
            .type = proto::SignalType::HISTOGRAM_VALUE,
            .name_hash = base::HashMetricName(kEnumHistogram),
            .time = clock_.Now() - base::Hours(3),
            .value = 2},
        SignalDatabase::DbEntry{
            .type = proto::SignalType::HISTOGRAM_VALUE,
            .name_hash = base::HashMetricName(kEnumHistogram),
            .time = clock_.Now() - base::Hours(3),
            .value = 3},
        SignalDatabase::DbEntry{
            .type = proto::SignalType::HISTOGRAM_VALUE,
            .name_hash = base::HashMetricName(kEnumHistogram),
            .time = clock_.Now() - base::Hours(1),
            .value = 4},
        SignalDatabase::DbEntry{
            .type = proto::SignalType::HISTOGRAM_VALUE,
            .name_hash = base::HashMetricName(kEnumHistogram),
            .time = clock_.Now(),
            .value = 5},

        SignalDatabase::DbEntry{
            .type = proto::SignalType::HISTOGRAM_ENUM,
            .name_hash = base::HashMetricName(kEnumHistogram),
            .time = clock_.Now() - base::Hours(3),
            .value = 1},
        SignalDatabase::DbEntry{
            .type = proto::SignalType::HISTOGRAM_ENUM,
            .name_hash = base::HashMetricName(kEnumHistogram),
            .time = clock_.Now() - base::Hours(3),
            .value = 2},
        SignalDatabase::DbEntry{
            .type = proto::SignalType::HISTOGRAM_ENUM,
            .name_hash = base::HashMetricName(kEnumHistogram),
            .time = clock_.Now() - base::Hours(3),
            .value = 3},
        SignalDatabase::DbEntry{
            .type = proto::SignalType::HISTOGRAM_ENUM,
            .name_hash = base::HashMetricName(kEnumHistogram),
            .time = clock_.Now() - base::Hours(1),
            .value = 4},
        SignalDatabase::DbEntry{
            .type = proto::SignalType::HISTOGRAM_ENUM,
            .name_hash = base::HashMetricName(kEnumHistogram),
            .time = clock_.Now(),
            .value = 5},
    };
  }

  void VerifySignalSampleViewQuery(const SignalSampleView::Query& query,
                                   std::vector<size_t> expected_indices) {
    std::vector<SignalDatabase::DbEntry> samples = GetDatabaseSamples();
    SignalSampleView view(samples, query);

    EXPECT_EQ(view.size(), expected_indices.size());
    EXPECT_FALSE(view.empty());
    std::vector<size_t> indices;
    for (auto it = view.begin(); it != view.end(); ++it) {
      indices.push_back(it.current());
      EXPECT_EQ(it->type, query.type);
      EXPECT_EQ(it->name_hash, query.metric_hash);
    }
    EXPECT_EQ(indices, expected_indices);
    EXPECT_EQ(view.Last().current(), *expected_indices.rbegin());
  }

 protected:
  base::SimpleTestClock clock_;
};

TEST_F(SignalSampleViewTest, SignalSampleViewEmpty) {
  std::vector<SignalDatabase::DbEntry> samples;
  SignalSampleView view1(samples, std::nullopt);

  EXPECT_EQ(view1.size(), 0u);
  EXPECT_TRUE(view1.empty());
  EXPECT_EQ(view1.begin(), view1.end());
  EXPECT_EQ(view1.Last(), view1.end());

  SignalSampleView view2(
      samples, SignalSampleView::Query(proto::SignalType::USER_ACTION,
                                       base::HashMetricName(kUserAction),
                                       clock_.Now() - base::Hours(50),
                                       clock_.Now(), {}));
  EXPECT_EQ(view2.size(), 0u);
  EXPECT_TRUE(view2.empty());
  EXPECT_EQ(view2.begin(), view2.end());
  EXPECT_EQ(view2.Last(), view2.end());
}

TEST_F(SignalSampleViewTest, SignalSampleViewQueryMissingAction) {
  std::vector<SignalDatabase::DbEntry> samples = GetDatabaseSamples();
  SignalSampleView view(
      samples, SignalSampleView::Query(proto::SignalType::USER_ACTION,
                                       base::HashMetricName("MissingAction"),
                                       clock_.Now() - base::Hours(50),
                                       clock_.Now(), {}));

  EXPECT_EQ(view.size(), 0u);
  EXPECT_TRUE(view.empty());
  EXPECT_EQ(view.begin(), view.end());
  EXPECT_EQ(view.Last(), view.end());
}

TEST_F(SignalSampleViewTest, SignalSampleViewWithoutQuery) {
  auto samples = GetDatabaseSamples();
  SignalSampleView view(samples, std::nullopt);

  EXPECT_EQ(view.size(), samples.size());
  EXPECT_FALSE(view.empty());
  size_t count = 0;
  for (auto it = view.begin(); it != view.end(); ++it, ++count) {
    EXPECT_EQ(it.current(), count);
    EXPECT_NE(it->type, proto::SignalType::UNKNOWN_SIGNAL_TYPE);
    EXPECT_GT(it->name_hash, 0u);
  }
  EXPECT_EQ(count, samples.size());
  EXPECT_EQ(view.Last().current(), samples.size() - 1);
}

TEST_F(SignalSampleViewTest, SignalSampleViewQueryActions) {
  std::vector<SignalDatabase::DbEntry> samples = GetDatabaseSamples();
  VerifySignalSampleViewQuery(
      SignalSampleView::Query(proto::SignalType::USER_ACTION,
                              base::HashMetricName(kUserAction),
                              clock_.Now() - base::Hours(50), clock_.Now(), {}),
      {4, 5, 6, 7});
}

TEST_F(SignalSampleViewTest, SignalSampleViewHistogram) {
  std::vector<SignalDatabase::DbEntry> samples = GetDatabaseSamples();
  // Use enum hash as value.
  VerifySignalSampleViewQuery(
      SignalSampleView::Query(proto::SignalType::HISTOGRAM_VALUE,
                              base::HashMetricName(kEnumHistogram),
                              clock_.Now() - base::Hours(50), clock_.Now(), {}),
      {15, 16, 17, 18, 19});
}

TEST_F(SignalSampleViewTest, SignalSampleViewEnumFiltering) {
  std::vector<SignalDatabase::DbEntry> samples = GetDatabaseSamples();
  const uint64_t enum_hash = base::HashMetricName(kEnumHistogram);
  VerifySignalSampleViewQuery(
      SignalSampleView::Query(proto::SignalType::HISTOGRAM_ENUM, enum_hash,
                              clock_.Now() - base::Hours(3), clock_.Now(), {}),
      {20, 21, 22, 23, 24});

  VerifySignalSampleViewQuery(
      SignalSampleView::Query(proto::SignalType::HISTOGRAM_ENUM, enum_hash,
                              clock_.Now() - base::Hours(3), clock_.Now(),
                              {1, 3, 5}),
      {20, 22, 24});

  VerifySignalSampleViewQuery(
      SignalSampleView::Query(proto::SignalType::HISTOGRAM_ENUM, enum_hash,
                              clock_.Now() - base::Hours(2), clock_.Now(),
                              {1, 4}),
      {23});
}

}  // namespace segmentation_platform
