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

// Set the first active week from VPD field as "2000-01".
// This value represents the UTC based activate date of the device formatted
// YYYY-WW to reduce privacy granularity.
// See
// https://crsrc.org/o/src/third_party/chromiumos-overlay/chromeos-base/chromeos-activate-date/files/activate_date;l=67
const char kFakeFirstActivateDate[] = "2000-01";

// The inception month for the active status value is Jan 2000.
const char kInceptionTimeString[] = "01 Jan 2000 00:00:00 GMT";

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
  ChurnActiveStatusBase() : ChurnActiveStatusBase(0) {}
  explicit ChurnActiveStatusBase(int active_status_val) {
    // Set the fake instance of statistics provider before
    // initializing churn active status.
    system::StatisticsProvider::SetTestProvider(&statistics_provider_);

    // Initialize base value of ActivateDate vpd field as
    // kFakeFirstActivateDate.
    statistics_provider_.SetMachineStatistic(system::kActivateDateKey,
                                             kFakeFirstActivateDate);

    churn_active_status_ =
        std::make_unique<ChurnActiveStatus>(active_status_val);
  }
  ChurnActiveStatusBase(const ChurnActiveStatusBase&) = delete;
  ChurnActiveStatusBase& operator=(const ChurnActiveStatusBase&) = delete;
  ~ChurnActiveStatusBase() override = default;

 protected:
  ChurnActiveStatus* GetChurnActiveStatus() {
    DCHECK(churn_active_status_);
    return churn_active_status_.get();
  }

  system::FakeStatisticsProvider statistics_provider_;

 private:
  // Object under test.
  std::unique_ptr<ChurnActiveStatus> churn_active_status_;
};

TEST_F(ChurnActiveStatusBase, ActivateDateWeeksIsLargerThanLimit) {
  // Overwrite the ActivateDate key in machine statistics.
  // Number of weeks "54" is invalid and should not be a possible value.
  statistics_provider_.SetMachineStatistic(system::kActivateDateKey, "2023-54");

  // Validate that default constructor of base::Time() is returned when number
  // of weeks is invalid.
  EXPECT_EQ(GetChurnActiveStatus()->GetFirstActiveWeekForTesting("2023", "54"),
            base::Time());
}

TEST_F(ChurnActiveStatusBase, InvalidFormatActivateDate) {
  // Overwrite the ActivateDate key in machine statistics.
  // ActivateDate value should be formatted as numbers with YYYY-WW.
  statistics_provider_.SetMachineStatistic(system::kActivateDateKey, "XXXX-XX");

  // Validate that default constructor of base::Time() is returned when invalid
  // input is passed.
  EXPECT_EQ(GetChurnActiveStatus()->GetFirstActiveWeekForTesting("XXXX", "XX"),
            base::Time());
}

TEST_F(ChurnActiveStatusBase, ZeroWeeksUntilFirstMondayOfYear) {
  // Overwrite the ActivateDate key in machine statistics.
  statistics_provider_.SetMachineStatistic(system::kActivateDateKey, "2023-00");

  base::Time expected_ts;
  EXPECT_TRUE(base::Time::FromString("Jan 01 00:00:00 GMT 2023", &expected_ts));

  // Validate activate date for week 0 of 2023 is Jan 01, 2023.
  // Note this date is not a monday since 00 week of year represents
  // a non-monday date in ActivateDate.
  EXPECT_EQ(GetChurnActiveStatus()->GetFirstActiveWeekForTesting("2023", "00"),
            expected_ts);
}

TEST_F(ChurnActiveStatusBase, FirstDayOfYearIsMonday) {
  // Overwrite the ActivateDate key in machine statistics.
  statistics_provider_.SetMachineStatistic(system::kActivateDateKey, "1996-01");

  base::Time expected_ts;
  EXPECT_TRUE(
      base::Time::FromString("Mon Jan 01 00:00:00 GMT 1996", &expected_ts));

  EXPECT_EQ(GetChurnActiveStatus()->GetFirstActiveWeekForTesting("1996", "01"),
            expected_ts);
}

TEST_F(ChurnActiveStatusBase, ValidateCertainMonthsHaveFiftyThreeWeeks) {
  // Overwrite the ActivateDate key in machine statistics.
  statistics_provider_.SetMachineStatistic(system::kActivateDateKey, "1996-53");

  base::Time expected_ts;
  EXPECT_TRUE(
      base::Time::FromString("Mon Dec 30 00:00:00 GMT 1996", &expected_ts));

  EXPECT_EQ(GetChurnActiveStatus()->GetFirstActiveWeekForTesting("1996", "53"),
            expected_ts);
}

TEST_F(ChurnActiveStatusBase, GetNewYearDateIfFirstDayOfYearIsNotMonday) {
  // Overwrite the ActivateDate key in machine statistics.
  statistics_provider_.SetMachineStatistic(system::kActivateDateKey, "1997-00");

  base::Time expected_ts;
  EXPECT_TRUE(
      base::Time::FromString("Mon Jan 01 00:00:00 GMT 1997", &expected_ts));

  EXPECT_EQ(GetChurnActiveStatus()->GetFirstActiveWeekForTesting("1997", "00"),
            expected_ts);
}

TEST_F(ChurnActiveStatusBase, GetFirstMondayIfFirstDayOfYearIsNotMonday) {
  // Overwrite the ActivateDate key in machine statistics.
  statistics_provider_.SetMachineStatistic(system::kActivateDateKey, "1997-01");

  base::Time expected_ts;
  EXPECT_TRUE(
      base::Time::FromString("Mon Jan 06 00:00:00 GMT 1997", &expected_ts));

  EXPECT_EQ(GetChurnActiveStatus()->GetFirstActiveWeekForTesting("1997", "01"),
            expected_ts);
}

TEST_F(ChurnActiveStatusBase, ValidateIso8601WeekBetween1And52) {
  // Overwrite the ActivateDate key in machine statistics.
  // Year 1996 has 53 ISO 8601 weeks standard in it.
  statistics_provider_.SetMachineStatistic(system::kActivateDateKey, "1996-52");

  base::Time expected_ts;
  EXPECT_TRUE(
      base::Time::FromString("Mon Dec 23 00:00:00 GMT 1996", &expected_ts));

  EXPECT_EQ(GetChurnActiveStatus()->GetFirstActiveWeekForTesting("1996", "52"),
            expected_ts);

  // Validate 1997 week 2 in ISO 8601 week standard.
  statistics_provider_.SetMachineStatistic(system::kActivateDateKey, "1997-02");
  base::Time expected_ts2;
  EXPECT_TRUE(
      base::Time::FromString("Mon Jan 13 00:00:00 GMT 1997", &expected_ts2));

  EXPECT_EQ(GetChurnActiveStatus()->GetFirstActiveWeekForTesting("1997", "02"),
            expected_ts2);

  // Validate 1997 week 52 in ISO 8601 week standard.
  statistics_provider_.SetMachineStatistic(system::kActivateDateKey, "1997-52");
  base::Time expected_ts3;
  EXPECT_TRUE(
      base::Time::FromString("Mon Dec 29 00:00:00 GMT 1997", &expected_ts3));

  EXPECT_EQ(GetChurnActiveStatus()->GetFirstActiveWeekForTesting("1997", "52"),
            expected_ts3);
}

TEST_F(ChurnActiveStatusBase, FirstMondayOfYear) {
  // Overwrite the ActivateDate key in machine statistics.
  statistics_provider_.SetMachineStatistic(system::kActivateDateKey, "2023-01");

  base::Time expected_ts;
  EXPECT_TRUE(
      base::Time::FromString("Mon Jan 02 00:00:00 GMT 2023", &expected_ts));

  // Validate activate date for week 1 of 2023 is Monday Jan 02, 2023.
  EXPECT_EQ(GetChurnActiveStatus()->GetFirstActiveWeekForTesting("2023", "01"),
            expected_ts);
}

TEST_F(ChurnActiveStatusBase, TenthWeekOfYear) {
  // Overwrite the ActivateDate key in machine statistics.
  statistics_provider_.SetMachineStatistic(system::kActivateDateKey, "2023-10");

  base::Time expected_ts;
  EXPECT_TRUE(
      base::Time::FromString("Mon Mar 06 00:00:00 GMT 2023", &expected_ts));

  // Validate activate date for week 10 of 2023 is Monday Mar 06, 2023.
  EXPECT_EQ(GetChurnActiveStatus()->GetFirstActiveWeekForTesting("2023", "10"),
            expected_ts);
}

TEST_F(ChurnActiveStatusBase, ActivateWeekExceedsFiftyTwo) {
  // Overwrite the ActivateDate key in machine statistics.
  statistics_provider_.SetMachineStatistic(system::kActivateDateKey, "2023-53");

  base::Time expected_ts;
  EXPECT_TRUE(
      base::Time::FromString("Mon Jan 01 00:00:00 GMT 2024", &expected_ts));

  // Validate activate date for week 53 of 2023 is Monday Jan 01, 2024.
  EXPECT_EQ(GetChurnActiveStatus()->GetFirstActiveWeekForTesting("2023", "53"),
            expected_ts);
}

TEST_F(ChurnActiveStatusBase, ActivateDateWeekNeverSpansTwoYears) {
  // Validate expected_ts does not go over year boundary.
  statistics_provider_.SetMachineStatistic(system::kActivateDateKey, "2019-52");
  base::Time expected_ts;
  EXPECT_TRUE(
      base::Time::FromString("Mon Dec 30 00:00:00 GMT 2019", &expected_ts));
  EXPECT_EQ(GetChurnActiveStatus()->GetFirstActiveWeekForTesting("2019", "52"),
            expected_ts);

  // Validate expected_ts2 doesn't go prior to current year date.
  statistics_provider_.SetMachineStatistic(system::kActivateDateKey, "2020-00");
  base::Time expected_ts2;
  EXPECT_TRUE(
      base::Time::FromString("Mon Jan 01 00:00:00 GMT 2020", &expected_ts2));
  EXPECT_EQ(GetChurnActiveStatus()->GetFirstActiveWeekForTesting("2020", "00"),
            expected_ts2);

  // Validate expected_ts3 does not go over year boundary.
  statistics_provider_.SetMachineStatistic(system::kActivateDateKey, "2020-52");
  base::Time expected_ts3;
  EXPECT_TRUE(
      base::Time::FromString("Mon Dec 28 00:00:00 GMT 2020", &expected_ts3));
  EXPECT_EQ(GetChurnActiveStatus()->GetFirstActiveWeekForTesting("2020", "52"),
            expected_ts3);
}

TEST_F(ChurnActiveStatusBase, ActivateDateWeekAtEndOfYear) {
  // Overwrite the ActivateDate key in machine statistics.
  statistics_provider_.SetMachineStatistic(system::kActivateDateKey, "2020-52");

  base::Time expected_ts;
  EXPECT_TRUE(
      base::Time::FromString("Mon Dec 28 00:00:00 GMT 2020", &expected_ts));

  // Validate activate date for week 52 of 2020 is Monday Dec 30, 2020.
  EXPECT_EQ(GetChurnActiveStatus()->GetFirstActiveWeekForTesting("2020", "52"),
            expected_ts);
}

TEST_F(ChurnActiveStatusBase, ActivateDateWeekSpansTwoMonths) {
  // Overwrite the ActivateDate key in machine statistics.
  statistics_provider_.SetMachineStatistic(system::kActivateDateKey, "2020-04");

  base::Time expected_ts;
  EXPECT_TRUE(
      base::Time::FromString("Mon Jan 27 00:00:00 GMT 2020", &expected_ts));

  // Validate activate date for week 4 of 2020 is Monday Jan 27, 2020.
  EXPECT_EQ(GetChurnActiveStatus()->GetFirstActiveWeekForTesting("2020", "04"),
            expected_ts);
}

class ChurnActiveStatusAtInceptionDate : public ChurnActiveStatusBase {
 public:
  // Decimal of 28 bits representing January 2000.
  // 0000000000 = 0 months from January 2000.
  // 000000000000000000 = No active months
  // 0000000000000000000000000000 = 0
  static constexpr int kInceptionDate = 0;

  ChurnActiveStatusAtInceptionDate() : ChurnActiveStatusBase(kInceptionDate) {}
};

TEST_F(ChurnActiveStatusAtInceptionDate, GetDefaultConstructorValue) {
  EXPECT_EQ(GetChurnActiveStatus()->GetValueAsInt(), 0);
}

TEST_F(ChurnActiveStatusAtInceptionDate, SetValueWithInt) {
  GetChurnActiveStatus()->SetValue(1);

  EXPECT_EQ(GetChurnActiveStatus()->GetActiveMonthBits(), 1);
  EXPECT_EQ(GetChurnActiveStatus()->GetMonthsSinceInception(), 0);
}

TEST_F(ChurnActiveStatusAtInceptionDate, SetValueWithMax28BitValAsInt) {
  int max_28_bit_as_int = 268435455;
  GetChurnActiveStatus()->SetValue(max_28_bit_as_int);

  // Months since inception and Active months bits should be set correctly.
  EXPECT_EQ(GetChurnActiveStatus()->GetMonthsSinceInception(),
            std::pow(2, 10) - 1);
  EXPECT_EQ(GetChurnActiveStatus()->GetActiveMonthBits(), std::pow(2, 18) - 1);
}

TEST_F(ChurnActiveStatusAtInceptionDate, ValidateTimestampForInceptionDate) {
  base::Time month;
  EXPECT_TRUE(base::Time::FromString(kInceptionTimeString, &month));

  EXPECT_EQ(GetChurnActiveStatus()->GetInceptionMonth(), month);
}

TEST_F(ChurnActiveStatusAtInceptionDate, SetMaxActiveStatusValueAsInt) {
  std::bitset<ChurnActiveStatus::kChurnBitSize> max_28_bits(
      "1111111111111111111111111111");
  const int max_28_bits_as_int = 268435455;
  EXPECT_EQ(ConvertBitSetToInt(max_28_bits), max_28_bits_as_int);

  ChurnActiveStatus* churn_active_status = GetChurnActiveStatus();
  churn_active_status->SetValue(max_28_bits);

  // Validate the max months since inception is:
  // 2^10 - 1, which is equal to 10 bits all turned on..
  EXPECT_EQ(churn_active_status->GetMonthsSinceInception(),
            std::pow(2, 10) - 1);

  // Validate the max active months bits is:
  // 2^18 - 1, which is equal to 18 bits all turned on.
  EXPECT_EQ(churn_active_status->GetActiveMonthBits(), std::pow(2, 18) - 1);
}

TEST_F(ChurnActiveStatusAtInceptionDate, GetMinActiveStatusValueAsInt) {
  std::bitset<ChurnActiveStatus::kChurnBitSize> min_28_bits(
      "0000000000000000000000000000");
  const int min_28_bits_as_int = 0;
  EXPECT_EQ(ConvertBitSetToInt(min_28_bits), min_28_bits_as_int);

  ChurnActiveStatus* churn_active_status = GetChurnActiveStatus();
  churn_active_status->SetValue(min_28_bits);

  // Validate the months since inception and active months are both 0.
  EXPECT_EQ(churn_active_status->GetMonthsSinceInception(), 0);
  EXPECT_EQ(churn_active_status->GetActiveMonthBits(), 0);
}

TEST_F(ChurnActiveStatusAtInceptionDate, ActiveMonthsBitsIsZero) {
  ChurnActiveStatus* churn_active_status = GetChurnActiveStatus();
  EXPECT_EQ(churn_active_status->GetActiveMonthBits(), 0);
}

TEST_F(ChurnActiveStatusAtInceptionDate, ActiveMonthsBitsIsMaxed) {
  ChurnActiveStatus* churn_active_status = GetChurnActiveStatus();
  EXPECT_EQ(churn_active_status->GetActiveMonthBits(), 0);

  base::Time month;
  EXPECT_TRUE(base::Time::FromString(kInceptionTimeString, &month));

  for (int exponent = 0; exponent < ChurnActiveStatus::kActiveMonthsBitSize;
       exponent++) {
    EXPECT_EQ(churn_active_status->GetActiveMonthBits(),
              std::pow(2, exponent) - 1);

    month = GetNextMonth(month);
    churn_active_status->UpdateValue(month);
  }

  // Verify that all bits are turned on for the past 18 months of active status.
  EXPECT_EQ(churn_active_status->GetActiveMonthBits(),
            std::pow(2, ChurnActiveStatus::kActiveMonthsBitSize) - 1);
}

TEST_F(ChurnActiveStatusAtInceptionDate,
       ActiveInceptionMonthDoesNotModifyValueBits) {
  ChurnActiveStatus* churn_active_status = GetChurnActiveStatus();
  EXPECT_EQ(churn_active_status->GetActiveMonthBits(), 0);

  // Inception month should not modify active bits.
  base::Time month;
  EXPECT_TRUE(base::Time::FromString(kInceptionTimeString, &month));

  churn_active_status->UpdateValue(month);
  EXPECT_EQ(churn_active_status->GetActiveMonthBits(), 0);
}

TEST_F(ChurnActiveStatusAtInceptionDate, UpdateActiveSameMonth) {
  ChurnActiveStatus* churn_active_status = GetChurnActiveStatus();
  EXPECT_EQ(churn_active_status->GetActiveMonthBits(), 0);

  base::Time month;
  EXPECT_TRUE(base::Time::FromString(kInceptionTimeString, &month));

  // Update active status for inception month + 1 month.
  // Inception month does not represent an active month.
  month = GetNextMonth(month);
  churn_active_status->UpdateValue(month);

  const int active_bits_val = churn_active_status->GetActiveMonthBits();
  const int expected_val = 1;

  EXPECT_EQ(active_bits_val, expected_val);

  // Attempt to update the value again using the same month.
  churn_active_status->UpdateValue(month);

  // Verify that same month does not modify active month bits.
  EXPECT_EQ(active_bits_val, expected_val);
}

TEST_F(ChurnActiveStatusAtInceptionDate, ValidateUpdatingActiveStatus) {
  ChurnActiveStatus* churn_active_status = GetChurnActiveStatus();

  base::Time month_1;
  EXPECT_TRUE(base::Time::FromString("2000-02-01T00:00:00Z", &month_1));
  churn_active_status->UpdateValue(month_1);
  std::bitset<ChurnActiveStatus::kMonthsSinceInceptionSize>
      expected_months_count("0000000001");
  std::bitset<ChurnActiveStatus::kActiveMonthsBitSize> expected_active_months(
      "000000000000000001");
  EXPECT_EQ(churn_active_status->GetMonthsSinceInception(),
            ConvertBitSetToInt(expected_months_count));
  EXPECT_EQ(churn_active_status->GetActiveMonthBits(),
            ConvertBitSetToInt(expected_active_months));

  base::Time month_2;
  EXPECT_TRUE(base::Time::FromString("2000-03-01T00:00:00Z", &month_2));
  churn_active_status->UpdateValue(month_2);
  expected_months_count =
      std::bitset<ChurnActiveStatus::kMonthsSinceInceptionSize>("0000000010");
  expected_active_months = std::bitset<ChurnActiveStatus::kActiveMonthsBitSize>(
      "000000000000000011");
  EXPECT_EQ(churn_active_status->GetMonthsSinceInception(),
            ConvertBitSetToInt(expected_months_count));
  EXPECT_EQ(churn_active_status->GetActiveMonthBits(),
            ConvertBitSetToInt(expected_active_months));

  base::Time month_3;
  EXPECT_TRUE(base::Time::FromString("2000-06-01T00:00:00Z", &month_3));
  churn_active_status->UpdateValue(month_3);
  expected_months_count =
      std::bitset<ChurnActiveStatus::kMonthsSinceInceptionSize>("0000000101");
  expected_active_months = std::bitset<ChurnActiveStatus::kActiveMonthsBitSize>(
      "000000000000011001");
  EXPECT_EQ(churn_active_status->GetMonthsSinceInception(),
            ConvertBitSetToInt(expected_months_count));
  EXPECT_EQ(churn_active_status->GetActiveMonthBits(),
            ConvertBitSetToInt(expected_active_months));
}

class ChurnActiveStatusAtFixedDate : public ChurnActiveStatusBase {
 public:
  // Decimal of 28 bits representing January 2023.
  // 0100010100 = 276 months since January 2000
  // 000000000000000001 = Only active in January 2023.
  // 0100010100000000000000000001 = 72351745
  static constexpr int kFixedDate = 72351745;
  static constexpr char kFixedDateTimeString[] = "01 Jan 2023 00:00:00 GMT";

  ChurnActiveStatusAtFixedDate() : ChurnActiveStatusBase(kFixedDate) {}
};

TEST_F(ChurnActiveStatusAtFixedDate, ValidateCurrentActiveMonth) {
  ChurnActiveStatus* churn_active_status = GetChurnActiveStatus();

  base::Time month;
  EXPECT_TRUE(base::Time::FromString(kFixedDateTimeString, &month));

  EXPECT_EQ(churn_active_status->GetCurrentActiveMonth(), month);
}

TEST_F(ChurnActiveStatusAtFixedDate, UpdateActiveBefore18Months) {
  ChurnActiveStatus* churn_active_status = GetChurnActiveStatus();

  // Set month_1 to exactly 17 months after January 2023.
  base::Time month_1;
  EXPECT_TRUE(base::Time::FromString("2024-06-01T00:00:00Z", &month_1));
  churn_active_status->UpdateValue(month_1);
  std::bitset<ChurnActiveStatus::kMonthsSinceInceptionSize>
      expected_months_count("0100100101");
  std::bitset<ChurnActiveStatus::kActiveMonthsBitSize> expected_active_months(
      "100000000000000001");
  EXPECT_EQ(churn_active_status->GetMonthsSinceInception(),
            ConvertBitSetToInt(expected_months_count));
  EXPECT_EQ(churn_active_status->GetActiveMonthBits(),
            ConvertBitSetToInt(expected_active_months));
}

TEST_F(ChurnActiveStatusAtFixedDate, UpdateActiveAt18Months) {
  ChurnActiveStatus* churn_active_status = GetChurnActiveStatus();

  // Set month_1 to exactly 18 months after January 2023.
  base::Time month_1;
  EXPECT_TRUE(base::Time::FromString("2024-07-01T00:00:00Z", &month_1));
  churn_active_status->UpdateValue(month_1);
  std::bitset<ChurnActiveStatus::kMonthsSinceInceptionSize>
      expected_months_count("0100100110");
  std::bitset<ChurnActiveStatus::kActiveMonthsBitSize> expected_active_months(
      "000000000000000001");
  EXPECT_EQ(churn_active_status->GetMonthsSinceInception(),
            ConvertBitSetToInt(expected_months_count));
  EXPECT_EQ(churn_active_status->GetActiveMonthBits(),
            ConvertBitSetToInt(expected_active_months));
}

TEST_F(ChurnActiveStatusAtFixedDate, UpdateActiveAfter18Months) {
  ChurnActiveStatus* churn_active_status = GetChurnActiveStatus();

  // Set month_1 to exactly 19 months after January 2023.
  base::Time month_1;
  EXPECT_TRUE(base::Time::FromString("2024-08-01T00:00:00Z", &month_1));
  churn_active_status->UpdateValue(month_1);
  std::bitset<ChurnActiveStatus::kMonthsSinceInceptionSize>
      expected_months_count("0100100111");
  std::bitset<ChurnActiveStatus::kActiveMonthsBitSize> expected_active_months(
      "000000000000000001");
  EXPECT_EQ(churn_active_status->GetMonthsSinceInception(),
            ConvertBitSetToInt(expected_months_count));
  EXPECT_EQ(churn_active_status->GetActiveMonthBits(),
            ConvertBitSetToInt(expected_active_months));
}

TEST_F(ChurnActiveStatusAtFixedDate, UpdateActiveFailsOnSettingBeforeDate) {
  ChurnActiveStatus* churn_active_status = GetChurnActiveStatus();

  // Set month_1 to 1 month prior to January 2023.
  // Validate that the |value_| does not change when trying to update with
  // a past date.
  base::Time month_1;
  EXPECT_TRUE(base::Time::FromString("2022-12-01T00:00:00Z", &month_1));
  EXPECT_EQ(churn_active_status->UpdateValue(month_1), absl::nullopt);

  // These two variables represent value for January 2023, NOT December 2022.
  std::bitset<ChurnActiveStatus::kMonthsSinceInceptionSize>
      expected_months_count("0100010100");
  std::bitset<ChurnActiveStatus::kActiveMonthsBitSize> expected_active_months(
      "000000000000000001");

  EXPECT_EQ(churn_active_status->GetMonthsSinceInception(),
            ConvertBitSetToInt(expected_months_count));
  EXPECT_EQ(churn_active_status->GetActiveMonthBits(),
            ConvertBitSetToInt(expected_active_months));
}

struct ChurnActiveStatusTestCase {
  // Name of specific test.
  std::string test_name;

  // 28 bit integer represents months from inception + 18 months of actives.
  int active_status_value;

  // As stored by the ActivateDate script.
  // Formatted: YYYY-WW
  std::string activate_date;

  // Expected values from given inputs.
  int expected_months_since_inception;

  std::string expected_current_active_month;

  int expected_active_month_bits;

  std::string expected_first_active_week;
};

class ChurnActiveStatusTest
    : public testing::TestWithParam<ChurnActiveStatusTestCase> {
 public:
  // Construct ChurnActiveStatusTest object with various ActiveStatus values.
  ChurnActiveStatusTest() {
    // Set the fake instance of statistics provider before
    // initializing churn active status.
    system::StatisticsProvider::SetTestProvider(&statistics_provider_);

    // Initialize value of ActivateDate vpd field as kFakeFirstActivateDate.
    statistics_provider_.SetMachineStatistic(system::kActivateDateKey,
                                             GetParam().activate_date);

    churn_active_status_ =
        std::make_unique<ChurnActiveStatus>(GetParam().active_status_value);
  }
  ChurnActiveStatusTest(const ChurnActiveStatusTest&) = delete;
  ChurnActiveStatusTest& operator=(const ChurnActiveStatusTest&) = delete;
  ~ChurnActiveStatusTest() override = default;

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

// Add parameterized tests for validating behaviour ChurnActiveStatus for
// different 28 bit ActiveStatus integer values.
INSTANTIATE_TEST_SUITE_P(
    VaryingActiveStatusValues,
    ChurnActiveStatusTest,
    testing::ValuesIn<ChurnActiveStatusTestCase>({
        {"FirstNewMonthFromInception", 262145, "2000-01", 1,
         "01 Feb 2000 00:00:00 GMT", 1, "03 Jan 2000 00:00:00 GMT"},
        {"ArbitraryDate1", 1310721, "2000-01", 5, "01 Jun 2000 00:00:00 GMT", 1,
         "03 Jan 2000 00:00:00 GMT"},
        {"ActiveValueGreaterThanVPDActivateDate", 72613889, "2023-10", 277,
         "01 Feb 2023 00:00:00 GMT", 1, "06 Mar 2023 00:00:00 GMT"},
    }),
    [](const testing::TestParamInfo<ChurnActiveStatusTestCase>& info) {
      return info.param.test_name;
    });

TEST_P(ChurnActiveStatusTest, ValidateGetCurrentActiveMonth) {
  ChurnActiveStatus* churn_active_status = GetChurnActiveStatus();

  // Validate the active status value passed to constructor is aligned with
  // expected value.
  EXPECT_EQ(churn_active_status->GetValueAsInt(),
            GetParam().active_status_value);

  // Validate inception month is always Jan 2000 GMT.
  base::Time inception_ts;
  EXPECT_TRUE(base::Time::FromUTCString(kInceptionTimeString, &inception_ts));
  EXPECT_EQ(churn_active_status->GetInceptionMonth(), inception_ts);

  // Validate expected months since inception.
  EXPECT_EQ(churn_active_status->GetMonthsSinceInception(),
            GetParam().expected_months_since_inception);

  // Validate current active month is aligned with the
  // expected current active month ts.
  base::Time current_active_month_ts;
  EXPECT_TRUE(base::Time::FromUTCString(
      GetParam().expected_current_active_month.c_str(),
      &current_active_month_ts));
  EXPECT_EQ(churn_active_status->GetCurrentActiveMonth(),
            current_active_month_ts);

  // Validate the active month bits from the 28 bit value is aligned with what
  // is expected.
  EXPECT_EQ(churn_active_status->GetActiveMonthBits(),
            GetParam().expected_active_month_bits);

  // Validate first active week is converted correctly based on ISO8601 week
  // format.
  base::Time first_active_week_ts;
  EXPECT_TRUE(base::Time::FromUTCString(
      GetParam().expected_first_active_week.c_str(), &first_active_week_ts));
  EXPECT_EQ(churn_active_status->GetFirstActiveWeek(), first_active_week_ts);
}

}  // namespace ash::device_activity
