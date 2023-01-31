// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/device_activity/churn_active_status.h"

#include "base/logging.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::device_activity {

namespace {

template <typename T>
int ConvertBitSetToInt(T bitset) {
  return static_cast<int>(bitset.to_ulong());
}

base::Time GetNextMonth(base::Time ts) {
  base::Time::Exploded exploded;
  ts.UTCExplode(&exploded);

  // Set new time to the first midnight of the next month.
  exploded.day_of_month = 1;
  exploded.month += 1;
  exploded.hour = 0;
  exploded.minute = 0;
  exploded.second = 0;
  exploded.millisecond = 0;

  // Handle case when month is december.
  if (exploded.month > 12) {
    exploded.year += 1;
    exploded.month = 1;
  }

  base::Time new_month_ts;
  EXPECT_TRUE(base::Time::FromUTCExploded(exploded, &new_month_ts));

  return new_month_ts;
}

}  // namespace

class ChurnActiveStatusBase : public testing::Test {
 public:
  explicit ChurnActiveStatusBase(int active_status_val) {
    // Set the fake instance of statistics provider before
    // initializing churn active status.
    system::StatisticsProvider::SetTestProvider(&statistics_provider_);

    churn_active_status_ =
        std::make_unique<ChurnActiveStatus>(active_status_val);
  }
  ChurnActiveStatusBase() = default;
  ChurnActiveStatusBase(const ChurnActiveStatusBase&) = delete;
  ChurnActiveStatusBase& operator=(const ChurnActiveStatusBase&) = delete;
  ~ChurnActiveStatusBase() override = default;

 protected:
  ChurnActiveStatus* GetChurnActiveStatus() {
    DCHECK(churn_active_status_);
    return churn_active_status_.get();
  }

 private:
  system::FakeStatisticsProvider statistics_provider_;

  // Object under test.
  std::unique_ptr<ChurnActiveStatus> churn_active_status_;
};

class ChurnActiveStatusAtInceptionDate : public ChurnActiveStatusBase {
 public:
  // Decimal of 28 bits representing January 2000.
  // 0000000000 = 0 months from January 2000.
  // 000000000000000000 = No active months
  // 0000000000000000000000000000 = 0
  static constexpr int kInceptionDate = 0;
  static constexpr char kInceptionTimeString[] = "01 Jan 2000 00:00:00 GMT";

  ChurnActiveStatusAtInceptionDate() : ChurnActiveStatusBase(kInceptionDate) {}
};

TEST_F(ChurnActiveStatusAtInceptionDate, GetDefaultConstructorValue) {
  EXPECT_EQ(GetChurnActiveStatus()->GetValueAsInt(), 0);
}

TEST_F(ChurnActiveStatusAtInceptionDate, SetMaxActiveStatusValueAsInt) {
  std::bitset<ChurnActiveStatus::kChurnBitSize> max_28_bits(
      "1111111111111111111111111111");
  const int max_28_bits_as_int = 268435455;
  EXPECT_EQ(ConvertBitSetToInt(max_28_bits), max_28_bits_as_int);

  ChurnActiveStatus* churn_active_status_ = GetChurnActiveStatus();
  churn_active_status_->SetValueForTesting(max_28_bits);

  // Validate the max months since inception is:
  // 2^10 - 1, which is equal to 10 bits all turned on..
  EXPECT_EQ(churn_active_status_->GetMonthsSinceInception(),
            std::pow(2, 10) - 1);

  // Validate the max active months bits is:
  // 2^18 - 1, which is equal to 18 bits all turned on.
  EXPECT_EQ(churn_active_status_->GetActiveMonthBits(), std::pow(2, 18) - 1);
}

TEST_F(ChurnActiveStatusAtInceptionDate, GetMinActiveStatusValueAsInt) {
  std::bitset<ChurnActiveStatus::kChurnBitSize> min_28_bits(
      "0000000000000000000000000000");
  const int min_28_bits_as_int = 0;
  EXPECT_EQ(ConvertBitSetToInt(min_28_bits), min_28_bits_as_int);

  ChurnActiveStatus* churn_active_status_ = GetChurnActiveStatus();
  churn_active_status_->SetValueForTesting(min_28_bits);

  // Validate the months since inception and active months are both 0.
  EXPECT_EQ(churn_active_status_->GetMonthsSinceInception(), 0);
  EXPECT_EQ(churn_active_status_->GetActiveMonthBits(), 0);
}

TEST_F(ChurnActiveStatusAtInceptionDate, ActiveMonthsBitsIsZero) {
  ChurnActiveStatus* churn_active_status_ = GetChurnActiveStatus();
  EXPECT_EQ(churn_active_status_->GetActiveMonthBits(), 0);
}

TEST_F(ChurnActiveStatusAtInceptionDate, ActiveMonthsBitsIsMaxed) {
  ChurnActiveStatus* churn_active_status_ = GetChurnActiveStatus();
  EXPECT_EQ(churn_active_status_->GetActiveMonthBits(), 0);

  base::Time month;
  EXPECT_TRUE(base::Time::FromString(kInceptionTimeString, &month));

  for (int exponent = 0; exponent < ChurnActiveStatus::kActiveMonthsBitSize;
       exponent++) {
    EXPECT_EQ(churn_active_status_->GetActiveMonthBits(),
              std::pow(2, exponent) - 1);

    month = GetNextMonth(month);
    churn_active_status_->UpdateValue(month);
  }

  // Verify that all bits are turned on for the past 18 months of active status.
  EXPECT_EQ(churn_active_status_->GetActiveMonthBits(),
            std::pow(2, ChurnActiveStatus::kActiveMonthsBitSize) - 1);
}

TEST_F(ChurnActiveStatusAtInceptionDate,
       ActiveInceptionMonthDoesNotModifyValueBits) {
  ChurnActiveStatus* churn_active_status_ = GetChurnActiveStatus();
  EXPECT_EQ(churn_active_status_->GetActiveMonthBits(), 0);

  // Inception month should not modify active bits.
  base::Time month;
  EXPECT_TRUE(base::Time::FromString(kInceptionTimeString, &month));

  churn_active_status_->UpdateValue(month);
  EXPECT_EQ(churn_active_status_->GetActiveMonthBits(), 0);
}

TEST_F(ChurnActiveStatusAtInceptionDate, UpdateActiveSameMonth) {
  ChurnActiveStatus* churn_active_status_ = GetChurnActiveStatus();
  EXPECT_EQ(churn_active_status_->GetActiveMonthBits(), 0);

  base::Time month;
  EXPECT_TRUE(base::Time::FromString(kInceptionTimeString, &month));

  // Update active status for inception month + 1 month.
  // Inception month does not represent an active month.
  month = GetNextMonth(month);
  churn_active_status_->UpdateValue(month);

  const int active_bits_val = churn_active_status_->GetActiveMonthBits();
  const int expected_val = 1;

  EXPECT_EQ(active_bits_val, expected_val);

  // Attempt to update the value again using the same month.
  churn_active_status_->UpdateValue(month);

  // Verify that same month does not modify active month bits.
  EXPECT_EQ(active_bits_val, expected_val);
}

TEST_F(ChurnActiveStatusAtInceptionDate, ValidateUpdatingActiveStatus) {
  ChurnActiveStatus* churn_active_status_ = GetChurnActiveStatus();

  base::Time month_1;
  EXPECT_TRUE(base::Time::FromString("2000-02-01T00:00:00Z", &month_1));
  churn_active_status_->UpdateValue(month_1);
  std::bitset<ChurnActiveStatus::kMonthsSinceInceptionSize>
      expected_months_count("0000000001");
  std::bitset<ChurnActiveStatus::kActiveMonthsBitSize> expected_active_months(
      "000000000000000001");
  EXPECT_EQ(churn_active_status_->GetMonthsSinceInception(),
            ConvertBitSetToInt(expected_months_count));
  EXPECT_EQ(churn_active_status_->GetActiveMonthBits(),
            ConvertBitSetToInt(expected_active_months));

  base::Time month_2;
  EXPECT_TRUE(base::Time::FromString("2000-03-01T00:00:00Z", &month_2));
  churn_active_status_->UpdateValue(month_2);
  expected_months_count =
      std::bitset<ChurnActiveStatus::kMonthsSinceInceptionSize>("0000000010");
  expected_active_months = std::bitset<ChurnActiveStatus::kActiveMonthsBitSize>(
      "000000000000000011");
  EXPECT_EQ(churn_active_status_->GetMonthsSinceInception(),
            ConvertBitSetToInt(expected_months_count));
  EXPECT_EQ(churn_active_status_->GetActiveMonthBits(),
            ConvertBitSetToInt(expected_active_months));

  base::Time month_3;
  EXPECT_TRUE(base::Time::FromString("2000-06-01T00:00:00Z", &month_3));
  churn_active_status_->UpdateValue(month_3);
  expected_months_count =
      std::bitset<ChurnActiveStatus::kMonthsSinceInceptionSize>("0000000101");
  expected_active_months = std::bitset<ChurnActiveStatus::kActiveMonthsBitSize>(
      "000000000000011001");
  EXPECT_EQ(churn_active_status_->GetMonthsSinceInception(),
            ConvertBitSetToInt(expected_months_count));
  EXPECT_EQ(churn_active_status_->GetActiveMonthBits(),
            ConvertBitSetToInt(expected_active_months));
}

class ChurnActiveStatusAtFixedDate : public ChurnActiveStatusBase {
 public:
  // Decimal of 28 bits representing January 2023.
  // 0100010100 = 276 months since January 2000
  // 000000000000000001 = Only active in January 2023.
  // 0100010100000000000000000001 = 72351745
  static constexpr int kFixedDate = 72351745;

  ChurnActiveStatusAtFixedDate() : ChurnActiveStatusBase(kFixedDate) {}
};

TEST_F(ChurnActiveStatusAtFixedDate, UpdateActiveBefore18Months) {
  ChurnActiveStatus* churn_active_status_ = GetChurnActiveStatus();

  // Set month_1 to exactly 17 months after January 2023.
  base::Time month_1;
  EXPECT_TRUE(base::Time::FromString("2024-06-01T00:00:00Z", &month_1));
  churn_active_status_->UpdateValue(month_1);
  std::bitset<ChurnActiveStatus::kMonthsSinceInceptionSize>
      expected_months_count("0100100101");
  std::bitset<ChurnActiveStatus::kActiveMonthsBitSize> expected_active_months(
      "100000000000000001");
  EXPECT_EQ(churn_active_status_->GetMonthsSinceInception(),
            ConvertBitSetToInt(expected_months_count));
  EXPECT_EQ(churn_active_status_->GetActiveMonthBits(),
            ConvertBitSetToInt(expected_active_months));
}

TEST_F(ChurnActiveStatusAtFixedDate, UpdateActiveAt18Months) {
  ChurnActiveStatus* churn_active_status_ = GetChurnActiveStatus();

  // Set month_1 to exactly 18 months after January 2023.
  base::Time month_1;
  EXPECT_TRUE(base::Time::FromString("2024-07-01T00:00:00Z", &month_1));
  churn_active_status_->UpdateValue(month_1);
  std::bitset<ChurnActiveStatus::kMonthsSinceInceptionSize>
      expected_months_count("0100100110");
  std::bitset<ChurnActiveStatus::kActiveMonthsBitSize> expected_active_months(
      "000000000000000001");
  EXPECT_EQ(churn_active_status_->GetMonthsSinceInception(),
            ConvertBitSetToInt(expected_months_count));
  EXPECT_EQ(churn_active_status_->GetActiveMonthBits(),
            ConvertBitSetToInt(expected_active_months));
}

TEST_F(ChurnActiveStatusAtFixedDate, UpdateActiveAfter18Months) {
  ChurnActiveStatus* churn_active_status_ = GetChurnActiveStatus();

  // Set month_1 to exactly 19 months after January 2023.
  base::Time month_1;
  EXPECT_TRUE(base::Time::FromString("2024-08-01T00:00:00Z", &month_1));
  churn_active_status_->UpdateValue(month_1);
  std::bitset<ChurnActiveStatus::kMonthsSinceInceptionSize>
      expected_months_count("0100100111");
  std::bitset<ChurnActiveStatus::kActiveMonthsBitSize> expected_active_months(
      "000000000000000001");
  EXPECT_EQ(churn_active_status_->GetMonthsSinceInception(),
            ConvertBitSetToInt(expected_months_count));
  EXPECT_EQ(churn_active_status_->GetActiveMonthBits(),
            ConvertBitSetToInt(expected_active_months));
}

TEST_F(ChurnActiveStatusAtFixedDate, UpdateActiveFailsOnSettingBeforeDate) {
  ChurnActiveStatus* churn_active_status_ = GetChurnActiveStatus();

  // Set month_1 to 1 month prior to January 2023.
  // Validate that the |value_| does not change when trying to update with
  // a past date.
  base::Time month_1;
  EXPECT_TRUE(base::Time::FromString("2022-12-01T00:00:00Z", &month_1));
  EXPECT_EQ(churn_active_status_->UpdateValue(month_1), absl::nullopt);

  // These two variables represent value for January 2023, NOT December 2022.
  std::bitset<ChurnActiveStatus::kMonthsSinceInceptionSize>
      expected_months_count("0100010100");
  std::bitset<ChurnActiveStatus::kActiveMonthsBitSize> expected_active_months(
      "000000000000000001");

  EXPECT_EQ(churn_active_status_->GetMonthsSinceInception(),
            ConvertBitSetToInt(expected_months_count));
  EXPECT_EQ(churn_active_status_->GetActiveMonthBits(),
            ConvertBitSetToInt(expected_active_months));
}

}  // namespace ash::device_activity
