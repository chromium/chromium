// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/autofill_dialog_models.h"

#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

namespace {

const base::Time kJune2017 = base::Time::FromDoubleT(1497552271);

}  // namespace

TEST(YearComboboxModelTest, ExpirationYear) {
  autofill::TestAutofillClock test_clock;
  test_clock.SetNow(kJune2017);

  YearComboboxModel model;
  ASSERT_EQ(11, model.GetItemCount());  // Placeholder + 2017-2026.
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_PLACEHOLDER_EXPIRY_YEAR),
      model.GetItemAt(0));
  EXPECT_EQ(base::ASCIIToUTF16("2017"), model.GetItemAt(1));
  EXPECT_EQ(base::ASCIIToUTF16("2018"), model.GetItemAt(2));
  EXPECT_EQ(base::ASCIIToUTF16("2019"), model.GetItemAt(3));
  EXPECT_EQ(base::ASCIIToUTF16("2020"), model.GetItemAt(4));
  EXPECT_EQ(base::ASCIIToUTF16("2021"), model.GetItemAt(5));
  EXPECT_EQ(base::ASCIIToUTF16("2022"), model.GetItemAt(6));
  EXPECT_EQ(base::ASCIIToUTF16("2023"), model.GetItemAt(7));
  EXPECT_EQ(base::ASCIIToUTF16("2024"), model.GetItemAt(8));
  EXPECT_EQ(base::ASCIIToUTF16("2025"), model.GetItemAt(9));
  EXPECT_EQ(base::ASCIIToUTF16("2026"), model.GetItemAt(10));
}

// Tests that we show the correct years, including an additional year.
TEST(YearComboboxModelTest, ShowAdditionalYear) {
  autofill::TestAutofillClock test_clock;
  test_clock.SetNow(kJune2017);

  YearComboboxModel model(2016);
  ASSERT_EQ(12, model.GetItemCount());  // Placeholder + 2016 + 2017-2026.
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_PLACEHOLDER_EXPIRY_YEAR),
      model.GetItemAt(0));
  EXPECT_EQ(base::ASCIIToUTF16("2016"), model.GetItemAt(1));
  EXPECT_EQ(base::ASCIIToUTF16("2017"), model.GetItemAt(2));
  EXPECT_EQ(base::ASCIIToUTF16("2018"), model.GetItemAt(3));
  EXPECT_EQ(base::ASCIIToUTF16("2019"), model.GetItemAt(4));
  EXPECT_EQ(base::ASCIIToUTF16("2020"), model.GetItemAt(5));
  EXPECT_EQ(base::ASCIIToUTF16("2021"), model.GetItemAt(6));
  EXPECT_EQ(base::ASCIIToUTF16("2022"), model.GetItemAt(7));
  EXPECT_EQ(base::ASCIIToUTF16("2023"), model.GetItemAt(8));
  EXPECT_EQ(base::ASCIIToUTF16("2024"), model.GetItemAt(9));
  EXPECT_EQ(base::ASCIIToUTF16("2025"), model.GetItemAt(10));
  EXPECT_EQ(base::ASCIIToUTF16("2026"), model.GetItemAt(11));
}

// Tests that we show the additional year, even if it is more than 10 years from
// now.
TEST(YearComboboxModelTest, ExpirationYear_ShowFarFutureYear) {
  autofill::TestAutofillClock test_clock;
  test_clock.SetNow(kJune2017);

  YearComboboxModel model(2042);
  ASSERT_EQ(12, model.GetItemCount());  // Placeholder + 2017-2026 + 2042.
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_PLACEHOLDER_EXPIRY_YEAR),
      model.GetItemAt(0));
  EXPECT_EQ(base::ASCIIToUTF16("2017"), model.GetItemAt(1));
  EXPECT_EQ(base::ASCIIToUTF16("2018"), model.GetItemAt(2));
  EXPECT_EQ(base::ASCIIToUTF16("2019"), model.GetItemAt(3));
  EXPECT_EQ(base::ASCIIToUTF16("2020"), model.GetItemAt(4));
  EXPECT_EQ(base::ASCIIToUTF16("2021"), model.GetItemAt(5));
  EXPECT_EQ(base::ASCIIToUTF16("2022"), model.GetItemAt(6));
  EXPECT_EQ(base::ASCIIToUTF16("2023"), model.GetItemAt(7));
  EXPECT_EQ(base::ASCIIToUTF16("2024"), model.GetItemAt(8));
  EXPECT_EQ(base::ASCIIToUTF16("2025"), model.GetItemAt(9));
  EXPECT_EQ(base::ASCIIToUTF16("2026"), model.GetItemAt(10));
  EXPECT_EQ(base::ASCIIToUTF16("2042"), model.GetItemAt(11));
}

TEST(YearComboboxModelTest, SetDefaultIndexByYear) {
  autofill::TestAutofillClock test_clock;
  test_clock.SetNow(kJune2017);

  YearComboboxModel model;
  model.SetDefaultIndexByYear(2017);
  ASSERT_EQ(base::ASCIIToUTF16("2017"),
            model.GetItemAt(model.GetDefaultIndex()));
}

TEST(YearComboboxModelTest, SetDefaultIndexByYearOutOfRange) {
  autofill::TestAutofillClock test_clock;
  test_clock.SetNow(kJune2017);

  YearComboboxModel model;
  model.SetDefaultIndexByYear(2016);
  ASSERT_EQ(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_PLACEHOLDER_EXPIRY_YEAR),
      model.GetItemAt(model.GetDefaultIndex()));
}

TEST(YearComboboxModelTest, SetDefaultIndexByYearAdditionalYear) {
  autofill::TestAutofillClock test_clock;
  test_clock.SetNow(kJune2017);

  YearComboboxModel model(2042);
  model.SetDefaultIndexByYear(2042);
  ASSERT_EQ(base::ASCIIToUTF16("2042"),
            model.GetItemAt(model.GetDefaultIndex()));
}

TEST(MonthComboboxModelTest, SetDefaultIndexByMonth) {
  MonthComboboxModel model;
  model.SetDefaultIndexByMonth(6);
  ASSERT_EQ(base::ASCIIToUTF16("06"), model.GetItemAt(model.GetDefaultIndex()));
}

TEST(MonthComboboxModelTest, SetDefaultIndexByMonthOutOfRange) {
  MonthComboboxModel model;
  model.SetDefaultIndexByMonth(13);
  ASSERT_EQ(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_PLACEHOLDER_EXPIRY_MONTH),
      model.GetItemAt(model.GetDefaultIndex()));
}

}  // namespace autofill
