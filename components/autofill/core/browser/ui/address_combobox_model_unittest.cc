// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/address_combobox_model.h"

#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/region_data.h"

namespace autofill {

namespace {
const char kAppLocale[] = "fr-CA";
}

TEST(AddressComboboxModelTest, Empty) {
  TestPersonalDataManager test_personal_data_manager;
  test_personal_data_manager.SetAutofillProfileEnabled(true);

  AddressComboboxModel model(test_personal_data_manager, kAppLocale, "");
  EXPECT_EQ(1, model.GetItemCount());
  EXPECT_FALSE(model.IsItemSeparatorAt(0));
  EXPECT_TRUE(model.GetItemIdentifierAt(0).empty());
  EXPECT_EQ(-1, model.GetIndexOfIdentifier("Anything"));
}

TEST(AddressComboboxModelTest, OneAddress) {
  TestPersonalDataManager test_personal_data_manager;
  test_personal_data_manager.SetAutofillProfileEnabled(true);
  AutofillProfile profile1(test::GetFullProfile());
  test_personal_data_manager.AddProfile(profile1);

  AddressComboboxModel model(test_personal_data_manager, kAppLocale,
                             profile1.guid());
  EXPECT_EQ(3, model.GetItemCount());
  EXPECT_FALSE(model.IsItemSeparatorAt(0));
  EXPECT_TRUE(model.IsItemSeparatorAt(1));
  EXPECT_TRUE(model.GetItemIdentifierAt(0).empty());
  EXPECT_TRUE(model.GetItemIdentifierAt(1).empty());
  EXPECT_EQ(-1, model.GetIndexOfIdentifier("Anything"));
  EXPECT_EQ(profile1.guid(), model.GetItemIdentifierAt(2));
  EXPECT_EQ(2, model.GetIndexOfIdentifier(profile1.guid()));
  EXPECT_EQ(2, model.GetDefaultIndex());
}

TEST(AddressComboboxModelTest, TwoAddresses) {
  TestPersonalDataManager test_personal_data_manager;
  test_personal_data_manager.SetAutofillProfileEnabled(true);
  AutofillProfile profile1(test::GetFullProfile());
  AutofillProfile profile2(test::GetFullProfile2());

  // Force |profile1| to be shown first in the combobox.
  profile1.set_use_count(100);
  test_personal_data_manager.AddProfile(profile1);
  test_personal_data_manager.AddProfile(profile2);

  AddressComboboxModel model(test_personal_data_manager, kAppLocale,
                             profile2.guid());
  EXPECT_EQ(4, model.GetItemCount());
  EXPECT_FALSE(model.IsItemSeparatorAt(0));
  EXPECT_TRUE(model.IsItemSeparatorAt(1));
  EXPECT_TRUE(model.GetItemIdentifierAt(0).empty());
  EXPECT_TRUE(model.GetItemIdentifierAt(1).empty());
  EXPECT_EQ(-1, model.GetIndexOfIdentifier("Anything"));
  EXPECT_EQ(profile1.guid(), model.GetItemIdentifierAt(2));
  EXPECT_EQ(profile2.guid(), model.GetItemIdentifierAt(3));
  EXPECT_EQ(2, model.GetIndexOfIdentifier(profile1.guid()));
  EXPECT_EQ(3, model.GetIndexOfIdentifier(profile2.guid()));
  EXPECT_EQ(3, model.GetDefaultIndex());
}

TEST(AddressComboboxModelTest, AddAnAddress) {
  TestPersonalDataManager test_personal_data_manager;
  test_personal_data_manager.SetAutofillProfileEnabled(true);
  AutofillProfile profile1(test::GetFullProfile());
  test_personal_data_manager.AddProfile(profile1);

  AddressComboboxModel model(test_personal_data_manager, kAppLocale, "");
  EXPECT_EQ(3, model.GetItemCount());
  EXPECT_EQ(profile1.guid(), model.GetItemIdentifierAt(2));
  EXPECT_EQ(2, model.GetIndexOfIdentifier(profile1.guid()));

  AutofillProfile profile2(test::GetFullProfile2());
  int new_profile_index = model.AddNewProfile(profile2);
  EXPECT_EQ(3, new_profile_index);
  EXPECT_EQ(4, model.GetItemCount());
  EXPECT_EQ(profile2.guid(), model.GetItemIdentifierAt(3));
  EXPECT_EQ(3, model.GetIndexOfIdentifier(profile2.guid()));

  // First profile shouldn't have changed, here the order is guaranteed.
  EXPECT_EQ(profile1.guid(), model.GetItemIdentifierAt(2));
  EXPECT_EQ(2, model.GetIndexOfIdentifier(profile1.guid()));
}

}  // namespace autofill
