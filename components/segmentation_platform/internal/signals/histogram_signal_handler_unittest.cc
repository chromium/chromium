// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/signals/histogram_signal_handler.h"

#include "base/metrics/histogram_macros.h"
#include "base/metrics/metrics_hashes.h"
#include "base/test/task_environment.h"
#include "components/segmentation_platform/internal/database/mock_signal_database.h"
#include "components/segmentation_platform/internal/database/mock_ukm_database.h"
#include "components/segmentation_platform/public/proto/types.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::segmentation_platform::proto::SignalType;
using ::testing::_;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::WithArgs;

namespace segmentation_platform {

namespace {

constexpr char kProfileId[] = "1";
constexpr char kExpectedHistogram[] = "some_histogram";
const uint64_t kExpectedHash = base::HashMetricName(kExpectedHistogram);
constexpr char kExpectedHistogram2[] = "other_histogram";
const uint64_t kExpectedHash2 = base::HashMetricName(kExpectedHistogram2);
constexpr char kExpectedHistogram3[] = "another_histogram";
const uint64_t kExpectedHash3 = base::HashMetricName(kExpectedHistogram3);

MATCHER_P2(SampleEq, type, hash, "") {
  return arg.type == type && arg.name_hash == hash;
}

class MockObserver : public HistogramSignalHandler::Observer {
 public:
  MockObserver() = default;
  ~MockObserver() override = default;
  MOCK_METHOD(void,
              OnHistogramSignalUpdated,
              (const std::string&, base::HistogramBase::Sample),
              (override));
};

class HistogramSignalHandlerTest : public testing::Test {
 public:
  HistogramSignalHandlerTest() = default;
  ~HistogramSignalHandlerTest() override = default;

  void SetUp() override {
    signal_database_ = std::make_unique<MockSignalDatabase>();
    ukm_db_ = std::make_unique<MockUkmDatabase>();
    histogram_signal_handler_ = std::make_unique<HistogramSignalHandler>(
        kProfileId, signal_database_.get(), ukm_db_.get());
  }

  void SetupHistograms() {
    std::set<std::pair<std::string, proto::SignalType>> histograms;
    histograms.insert(
        std::make_pair(kExpectedHistogram, proto::SignalType::HISTOGRAM_ENUM));
    histograms.insert(std::make_pair(kExpectedHistogram2,
                                     proto::SignalType::HISTOGRAM_VALUE));
    histogram_signal_handler_->SetRelevantHistograms(histograms);
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<MockSignalDatabase> signal_database_;
  std::unique_ptr<MockUkmDatabase> ukm_db_;
  std::unique_ptr<HistogramSignalHandler> histogram_signal_handler_;
  MockObserver observer_;
};

TEST_F(HistogramSignalHandlerTest, HistogramsAreRecorded) {
  // Initialize and register the list of histograms we are listening to.
  histogram_signal_handler_->EnableMetrics(true);
  SetupHistograms();

  // Record a registered histogram sample. It should be recorded.
  EXPECT_CALL(*signal_database_, WriteSample(proto::SignalType::HISTOGRAM_ENUM,
                                             kExpectedHash, Eq(1), _));
  EXPECT_CALL(*ukm_db_,
              AddUmaMetric(kProfileId, SampleEq(SignalType::HISTOGRAM_ENUM,
                                                kExpectedHash)));

  UMA_HISTOGRAM_BOOLEAN(kExpectedHistogram, true);
  task_environment_.RunUntilIdle();

  // Record an unrelated histogram sample. It should be ignored.
  std::string kUnrelatedHistogram = "unrelated_histogram";
  EXPECT_CALL(*signal_database_,
              WriteSample(_, base::HashMetricName(kUnrelatedHistogram), _, _))
      .Times(0);
  EXPECT_CALL(*ukm_db_,
              AddUmaMetric(kProfileId,
                           SampleEq(SignalType::HISTOGRAM_ENUM,
                                    base::HashMetricName(kUnrelatedHistogram))))
      .Times(0);
  UMA_HISTOGRAM_BOOLEAN(kUnrelatedHistogram, true);
  task_environment_.RunUntilIdle();

  std::set<std::pair<std::string, proto::SignalType>> histograms;
  histograms.insert(
      std::make_pair(kExpectedHistogram2, proto::SignalType::HISTOGRAM_ENUM));
  histograms.insert(
      std::make_pair(kExpectedHistogram3, proto::SignalType::HISTOGRAM_ENUM));
  histogram_signal_handler_->SetRelevantHistograms(histograms);

  EXPECT_CALL(*signal_database_, WriteSample(_, kExpectedHash, _, _)).Times(0);
  EXPECT_CALL(*ukm_db_,
              AddUmaMetric(kProfileId,
                           SampleEq(SignalType::HISTOGRAM_ENUM, kExpectedHash)))
      .Times(0);
  EXPECT_CALL(*signal_database_, WriteSample(_, kExpectedHash2, _, _));
  EXPECT_CALL(*ukm_db_,
              AddUmaMetric(kProfileId, SampleEq(SignalType::HISTOGRAM_ENUM,
                                                kExpectedHash2)));
  EXPECT_CALL(*signal_database_, WriteSample(_, kExpectedHash3, _, _));
  EXPECT_CALL(*ukm_db_,
              AddUmaMetric(kProfileId, SampleEq(SignalType::HISTOGRAM_ENUM,
                                                kExpectedHash3)));
  UMA_HISTOGRAM_BOOLEAN(kExpectedHistogram, true);
  UMA_HISTOGRAM_BOOLEAN(kExpectedHistogram2, true);
  UMA_HISTOGRAM_BOOLEAN(kExpectedHistogram3, true);
  task_environment_.RunUntilIdle();
}

TEST_F(HistogramSignalHandlerTest, HistogramAsValueAndEnum) {
  histogram_signal_handler_->EnableMetrics(true);

  // Add the same histogram as enum and as value type, the observer should write
  // 2 samples to database.
  std::set<std::pair<std::string, proto::SignalType>> histograms;
  histograms.insert(
      std::make_pair(kExpectedHistogram, proto::SignalType::HISTOGRAM_ENUM));
  histograms.insert(
      std::make_pair(kExpectedHistogram, proto::SignalType::HISTOGRAM_VALUE));
  histogram_signal_handler_->SetRelevantHistograms(histograms);

  // Record a registered histogram sample. It should be recorded.
  EXPECT_CALL(*signal_database_, WriteSample(proto::SignalType::HISTOGRAM_ENUM,
                                             kExpectedHash, Eq(1), _));
  EXPECT_CALL(*ukm_db_,
              AddUmaMetric(kProfileId, SampleEq(SignalType::HISTOGRAM_ENUM,
                                                kExpectedHash)));
  EXPECT_CALL(*signal_database_, WriteSample(proto::SignalType::HISTOGRAM_VALUE,
                                             kExpectedHash, Eq(1), _));
  EXPECT_CALL(*ukm_db_,
              AddUmaMetric(kProfileId, SampleEq(SignalType::HISTOGRAM_VALUE,
                                                kExpectedHash)));

  UMA_HISTOGRAM_BOOLEAN(kExpectedHistogram, true);
  task_environment_.RunUntilIdle();
}

TEST_F(HistogramSignalHandlerTest, DisableMetrics) {
  SetupHistograms();

  // Metrics is disabled on startup.
  EXPECT_CALL(*signal_database_, WriteSample(proto::SignalType::HISTOGRAM_ENUM,
                                             kExpectedHash, Eq(1), _))
      .Times(0);
  EXPECT_CALL(*ukm_db_,
              AddUmaMetric(kProfileId,
                           SampleEq(SignalType::HISTOGRAM_ENUM, kExpectedHash)))
      .Times(0);

  UMA_HISTOGRAM_BOOLEAN(kExpectedHistogram, true);
  task_environment_.RunUntilIdle();

  // Enable metrics.
  histogram_signal_handler_->EnableMetrics(true);
  EXPECT_CALL(*signal_database_, WriteSample(proto::SignalType::HISTOGRAM_ENUM,
                                             kExpectedHash, Eq(1), _))
      .Times(1);
  EXPECT_CALL(*ukm_db_,
              AddUmaMetric(kProfileId, SampleEq(SignalType::HISTOGRAM_ENUM,
                                                kExpectedHash)));
  UMA_HISTOGRAM_BOOLEAN(kExpectedHistogram, true);
  task_environment_.RunUntilIdle();

  // Disable metrics again.
  histogram_signal_handler_->EnableMetrics(false);
  EXPECT_CALL(*signal_database_, WriteSample(proto::SignalType::HISTOGRAM_ENUM,
                                             kExpectedHash, Eq(1), _))
      .Times(0);
  EXPECT_CALL(*ukm_db_,
              AddUmaMetric(kProfileId,
                           SampleEq(SignalType::HISTOGRAM_ENUM, kExpectedHash)))
      .Times(0);
  UMA_HISTOGRAM_BOOLEAN(kExpectedHistogram, true);
  task_environment_.RunUntilIdle();

  // Enable metrics again.
  histogram_signal_handler_->EnableMetrics(true);
  EXPECT_CALL(*signal_database_, WriteSample(proto::SignalType::HISTOGRAM_ENUM,
                                             kExpectedHash, Eq(1), _))
      .Times(1);
  EXPECT_CALL(*ukm_db_,
              AddUmaMetric(kProfileId, SampleEq(SignalType::HISTOGRAM_ENUM,
                                                kExpectedHash)));
  UMA_HISTOGRAM_BOOLEAN(kExpectedHistogram, true);
  task_environment_.RunUntilIdle();
}

TEST_F(HistogramSignalHandlerTest, ObserversNotified) {
  histogram_signal_handler_->EnableMetrics(true);
  SetupHistograms();
  histogram_signal_handler_->AddObserver(&observer_);

  EXPECT_CALL(*signal_database_, WriteSample(proto::SignalType::HISTOGRAM_ENUM,
                                             kExpectedHash, Eq(1), _))
      .WillOnce(
          WithArgs<3>(Invoke([](MockSignalDatabase::SuccessCallback callback) {
            std::move(callback).Run(true);
          })));
  EXPECT_CALL(*ukm_db_,
              AddUmaMetric(kProfileId, SampleEq(SignalType::HISTOGRAM_ENUM,
                                                kExpectedHash)));
  EXPECT_CALL(observer_,
              OnHistogramSignalUpdated(std::string(kExpectedHistogram), Eq(1)));

  // Record a registered histogram sample. |observer_| should be notified.
  UMA_HISTOGRAM_BOOLEAN(kExpectedHistogram, true);
  task_environment_.RunUntilIdle();

  histogram_signal_handler_->RemoveObserver(&observer_);
}

}  // namespace
}  // namespace segmentation_platform
