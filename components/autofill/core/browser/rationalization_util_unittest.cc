// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/rationalization_util.h"

#include <stddef.h>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;
using base::StringToInt;
using base::UTF8ToUTF16;

namespace autofill {

class AutofillRationalizationUtilTest : public testing::Test {};

TEST_F(AutofillRationalizationUtilTest, PhoneNumber_FirstNumberIsWholeNumber) {
  std::vector<AutofillField*> field_list;

  AutofillField field0;
  field0.SetTypeTo(AutofillType(NAME_FULL));
  field_list.push_back(&field0);

  AutofillField field1;
  field1.SetTypeTo(AutofillType(ADDRESS_HOME_LINE1));
  field_list.push_back(&field1);

  AutofillField field2;
  field2.SetTypeTo(AutofillType(PHONE_HOME_WHOLE_NUMBER));
  field_list.push_back(&field2);

  AutofillField field3;
  field3.SetTypeTo(AutofillType(PHONE_HOME_CITY_AND_NUMBER));
  field_list.push_back(&field3);

  rationalization_util::RationalizePhoneNumberFields(field_list);

  EXPECT_FALSE(field_list[0]->only_fill_when_focused());
  EXPECT_FALSE(field_list[1]->only_fill_when_focused());
  EXPECT_FALSE(field_list[2]->only_fill_when_focused());
  EXPECT_TRUE(field_list[3]->only_fill_when_focused());
}

TEST_F(AutofillRationalizationUtilTest,
       PhoneNumber_FirstNumberIsComponentized) {
  std::vector<AutofillField*> field_list;

  AutofillField field0;
  field0.SetTypeTo(AutofillType(NAME_FULL));
  field_list.push_back(&field0);

  AutofillField field1;
  field1.SetTypeTo(AutofillType(ADDRESS_HOME_LINE1));
  field_list.push_back(&field1);

  AutofillField field2;
  field2.max_length = 2;
  field2.SetTypeTo(AutofillType(PHONE_HOME_COUNTRY_CODE));
  field_list.push_back(&field2);

  AutofillField field3;
  field3.max_length = 3;
  field3.SetTypeTo(AutofillType(PHONE_HOME_CITY_CODE));
  field_list.push_back(&field3);

  AutofillField field4;
  field4.max_length = 7;
  field4.SetTypeTo(AutofillType(PHONE_HOME_NUMBER));
  field_list.push_back(&field4);

  AutofillField field5;
  field5.max_length = 2;
  field5.SetTypeTo(AutofillType(PHONE_HOME_COUNTRY_CODE));
  field_list.push_back(&field5);

  AutofillField field6;
  field6.max_length = 3;
  field6.SetTypeTo(AutofillType(PHONE_HOME_CITY_CODE));
  field_list.push_back(&field6);

  AutofillField field7;
  field7.max_length = 7;
  field7.SetTypeTo(AutofillType(PHONE_HOME_NUMBER));
  field_list.push_back(&field7);

  rationalization_util::RationalizePhoneNumberFields(field_list);

  EXPECT_FALSE(field_list[0]->only_fill_when_focused());
  EXPECT_FALSE(field_list[1]->only_fill_when_focused());
  EXPECT_FALSE(field_list[2]->only_fill_when_focused());
  EXPECT_FALSE(field_list[3]->only_fill_when_focused());
  EXPECT_FALSE(field_list[4]->only_fill_when_focused());

  EXPECT_TRUE(field_list[5]->only_fill_when_focused());
  EXPECT_TRUE(field_list[6]->only_fill_when_focused());
  EXPECT_TRUE(field_list[7]->only_fill_when_focused());
}

TEST_F(AutofillRationalizationUtilTest,
       PhoneNumber_BestEffortWhenNoCompleteNumberIsFound) {
  std::vector<AutofillField*> field_list;

  AutofillField field0;
  field0.SetTypeTo(AutofillType(NAME_FULL));
  field_list.push_back(&field0);

  AutofillField field1;
  field1.SetTypeTo(AutofillType(ADDRESS_HOME_LINE1));
  field_list.push_back(&field1);

  AutofillField field2;
  field2.SetTypeTo(AutofillType(PHONE_HOME_COUNTRY_CODE));
  field_list.push_back(&field2);

  AutofillField field3;
  field3.SetTypeTo(AutofillType(PHONE_HOME_CITY_CODE));
  field_list.push_back(&field3);

  rationalization_util::RationalizePhoneNumberFields(field_list);

  EXPECT_FALSE(field_list[0]->only_fill_when_focused());
  EXPECT_FALSE(field_list[1]->only_fill_when_focused());
  EXPECT_FALSE(field_list[2]->only_fill_when_focused());
  EXPECT_FALSE(field_list[3]->only_fill_when_focused());
}

TEST_F(AutofillRationalizationUtilTest, PhoneNumber_FillPhonePartsOnceOnly) {
  std::vector<AutofillField*> field_list;

  AutofillField field0;
  field0.SetTypeTo(AutofillType(NAME_FULL));
  field_list.push_back(&field0);

  AutofillField field1;
  field1.SetTypeTo(AutofillType(ADDRESS_HOME_LINE1));
  field_list.push_back(&field1);

  AutofillField field2;
  field2.SetTypeTo(AutofillType(PHONE_HOME_CITY_CODE));
  field_list.push_back(&field2);

  AutofillField field3;
  field3.max_length = 10;
  field3.SetTypeTo(AutofillType(PHONE_HOME_NUMBER));
  field_list.push_back(&field3);

  AutofillField field4;
  field4.max_length = 12;
  field4.SetTypeTo(AutofillType(PHONE_HOME_WHOLE_NUMBER));
  field_list.push_back(&field4);

  AutofillField field5;
  field5.SetTypeTo(AutofillType(PHONE_HOME_CITY_CODE));
  field_list.push_back(&field5);

  rationalization_util::RationalizePhoneNumberFields(field_list);

  EXPECT_FALSE(field_list[0]->only_fill_when_focused());
  EXPECT_FALSE(field_list[1]->only_fill_when_focused());
  EXPECT_FALSE(field_list[2]->only_fill_when_focused());
  EXPECT_FALSE(field_list[3]->only_fill_when_focused());
  EXPECT_TRUE(field_list[4]->only_fill_when_focused());
  EXPECT_TRUE(field_list[5]->only_fill_when_focused());
}

TEST_F(AutofillRationalizationUtilTest,
       PhoneNumber_SkipHiddenPhoneNumberFields) {
  std::vector<AutofillField*> field_list;

  AutofillField field0;
  field0.SetTypeTo(AutofillType(NAME_FULL));
  field_list.push_back(&field0);

  AutofillField field1;
  field1.SetTypeTo(AutofillType(ADDRESS_HOME_LINE1));
  field_list.push_back(&field1);

  AutofillField field2;
  field2.is_focusable = false;
  field2.SetTypeTo(AutofillType(PHONE_HOME_CITY_AND_NUMBER));
  field_list.push_back(&field2);

  AutofillField field3;
  field3.SetTypeTo(AutofillType(PHONE_HOME_WHOLE_NUMBER));
  field_list.push_back(&field3);

  rationalization_util::RationalizePhoneNumberFields(field_list);

  EXPECT_FALSE(field_list[0]->only_fill_when_focused());
  EXPECT_FALSE(field_list[1]->only_fill_when_focused());
  EXPECT_TRUE(field_list[2]->only_fill_when_focused());
  EXPECT_FALSE(field_list[3]->only_fill_when_focused());
}

}  // namespace autofill
