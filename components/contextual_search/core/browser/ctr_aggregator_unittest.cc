// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_search/core/browser/ctr_aggregator.h"

#include <memory>
#include <unordered_map>

#include "base/gtest_prod_util.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Value;

namespace {
const int kTestWeek = 2500;
}

namespace contextual_search {

class CtrAggregatorTest : public testing::Test {
 public:
  CtrAggregatorTest() {}

  CtrAggregatorTest(const CtrAggregatorTest&) = delete;
  CtrAggregatorTest& operator=(const CtrAggregatorTest&) = delete;

  ~CtrAggregatorTest() override {}

  class WeeklyActivityStorageStub : public WeeklyActivityStorage {
   public:
    WeeklyActivityStorageStub();

   private:
    int ReadClicksForWeekRemainder(int week_remainder) override;
    int ReadImpressionsForWeekRemainder(int week_remainder) override;
    int ReadOldestWeekWritten() override;
    int ReadNewestWeekWritten() override;
    void WriteClicksForWeekRemainder(int week_remainder, int value) override;
    void WriteImpressionsForWeekRemainder(int week_remainder,
                                          int value) override;
    void WriteOldestWeekWritten(int value) override;
    void WriteNewestWeekWritten(int value) override;

    std::unordered_map<int, int> clicks_;
    std::unordered_map<int, int> impressions_;
    int oldest_week_;
    int newest_week_;
  };

  // Test helpers
  void Fill4Weeks();  // Fill 4 weeks with 2 impressions, 1 click.

  // The class under test.
  std::unique_ptr<CtrAggregator> aggregator_;

 protected:
  // The storage stub.
  std::unique_ptr<WeeklyActivityStorage> storage_;

  void SetUp() override {
    storage_ = std::make_unique<WeeklyActivityStorageStub>();
    aggregator_.reset(new CtrAggregator(*storage_, kTestWeek));
  }

  void TearDown() override {}
};

CtrAggregatorTest::WeeklyActivityStorageStub::WeeklyActivityStorageStub()
    : WeeklyActivityStorage(4), oldest_week_(0), newest_week_(0) {}

int CtrAggregatorTest::WeeklyActivityStorageStub::ReadClicksForWeekRemainder(
    int week_remainder) {
  return clicks_[week_remainder];
}

int CtrAggregatorTest::WeeklyActivityStorageStub::
    ReadImpressionsForWeekRemainder(int week_remainder) {
  return impressions_[week_remainder];
}

int CtrAggregatorTest::WeeklyActivityStorageStub::ReadOldestWeekWritten() {
  return oldest_week_;
}

int CtrAggregatorTest::WeeklyActivityStorageStub::ReadNewestWeekWritten() {
  return newest_week_;
}

void CtrAggregatorTest::WeeklyActivityStorageStub::WriteClicksForWeekRemainder(
    int week_remainder,
    int value) {
  clicks_[week_remainder] = value;
}

void CtrAggregatorTest::WeeklyActivityStorageStub::
    WriteImpressionsForWeekRemainder(int week_remainder, int value) {
  impressions_[week_remainder] = value;
}

void CtrAggregatorTest::WeeklyActivityStorageStub::WriteOldestWeekWritten(
    int value) {
  oldest_week_ = value;
}

void CtrAggregatorTest::WeeklyActivityStorageStub::WriteNewestWeekWritten(
    int value) {
  newest_week_ = value;
}

void CtrAggregatorTest::Fill4Weeks() {
  int weeks_to_record = 4;
  for (int i = 0; i < weeks_to_record; i++) {
    aggregator_->RecordImpression(true);
    aggregator_->RecordImpression(false);
    EXPECT_FALSE(aggregator_->HasPrevious28DayData());
    aggregator_->IncrementWeek(1);
  }
  EXPECT_TRUE(aggregator_->HasPrevious28DayData());
}

// NaN has the property that it is not equal to itself.
#define EXPECT_NAN(x) EXPECT_NE(x, x)

TEST_F(CtrAggregatorTest, SimpleOperationTest) {
  aggregator_->RecordImpression(true);
  aggregator_->RecordImpression(false);
  EXPECT_FALSE(aggregator_->HasPreviousWeekData());
  EXPECT_EQ(0, aggregator_->GetPreviousWeekImpressions());
  EXPECT_NAN(aggregator_->GetPreviousWeekCtr());

  aggregator_->IncrementWeek(1);
  EXPECT_TRUE(aggregator_->HasPreviousWeekData());
  EXPECT_EQ(2, aggregator_->GetPreviousWeekImpressions());
  EXPECT_FLOAT_EQ(0.5f, aggregator_->GetPreviousWeekCtr());
}

TEST_F(CtrAggregatorTest, MultiWeekTest) {
  Fill4Weeks();
  aggregator_->RecordImpression(false);
  aggregator_->IncrementWeek(1);
  EXPECT_TRUE(aggregator_->HasPrevious28DayData());
  EXPECT_FLOAT_EQ(static_cast<float>(3.0 / 7),
                  aggregator_->GetPrevious28DayCtr());
  aggregator_->RecordImpression(false);
  aggregator_->IncrementWeek(1);
  EXPECT_TRUE(aggregator_->HasPrevious28DayData());
  EXPECT_FLOAT_EQ(static_cast<float>(2.0 / 6),
                  aggregator_->GetPrevious28DayCtr());
}

TEST_F(CtrAggregatorTest, SkipOneWeekTest) {
  Fill4Weeks();
  aggregator_->IncrementWeek(1);
  EXPECT_EQ(0, aggregator_->GetPreviousWeekCtr());
  EXPECT_FLOAT_EQ(static_cast<float>(3.0 / 6),
                  aggregator_->GetPrevious28DayCtr());
}

TEST_F(CtrAggregatorTest, SkipThreeWeeksTest) {
  Fill4Weeks();
  aggregator_->IncrementWeek(3);
  EXPECT_EQ(0, aggregator_->GetPreviousWeekCtr());
  EXPECT_FLOAT_EQ(static_cast<float>(1.0 / 2),
                  aggregator_->GetPrevious28DayCtr());
}

TEST_F(CtrAggregatorTest, SkipFourWeeksTest) {
  Fill4Weeks();
  aggregator_->IncrementWeek(4);
  EXPECT_EQ(0, aggregator_->GetPreviousWeekCtr());
  EXPECT_EQ(0, aggregator_->GetPrevious28DayCtr());
}

}  // namespace contextual_search
