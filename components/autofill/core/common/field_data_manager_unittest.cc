// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/field_data_manager.h"

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/unique_ids.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::FormFieldData;
using base::ASCIIToUTF16;
using base::UTF8ToUTF16;

namespace autofill {

class FieldDataManagerTest : public testing::Test {
 protected:
  void SetUp() override {
    FormFieldData field1;
    field1.set_id_attribute(u"name1");
    field1.set_value(u"first");
    field1.set_form_control_type(FormControlType::kInputText);
    field1.set_renderer_id(FieldRendererId(1));
    control_elements_.push_back(field1);

    FormFieldData field2;
    field2.set_id_attribute(u"name2");
    field2.set_form_control_type(FormControlType::kInputPassword);
    field2.set_renderer_id(FieldRendererId(2));
    control_elements_.push_back(field2);
  }

  void TearDown() override { control_elements_.clear(); }

  std::vector<FormFieldData> control_elements_;
};

TEST_F(FieldDataManagerTest, UpdateFieldDataMap) {
  const scoped_refptr<FieldDataManager> field_data_manager =
      base::MakeRefCounted<FieldDataManager>();
  field_data_manager->UpdateFieldDataMap(control_elements_[0].renderer_id(),
                                         control_elements_[0].value(),
                                         FieldPropertiesFlags::kUserTyped);
  const FieldRendererId id(control_elements_[0].renderer_id());
  EXPECT_TRUE(field_data_manager->HasFieldData(id));
  EXPECT_EQ(u"first", field_data_manager->GetUserInput(id));
  EXPECT_EQ(FieldPropertiesFlags::kUserTyped,
            field_data_manager->GetFieldPropertiesMask(id));

  field_data_manager->UpdateFieldDataMap(control_elements_[0].renderer_id(),
                                         u"newvalue",
                                         FieldPropertiesFlags::kAutofilled);
  EXPECT_EQ(u"newvalue", field_data_manager->GetUserInput(id));
  FieldPropertiesMask mask =
      FieldPropertiesFlags::kUserTyped | FieldPropertiesFlags::kAutofilled;
  EXPECT_EQ(mask, field_data_manager->GetFieldPropertiesMask(id));

  field_data_manager->UpdateFieldDataMap(control_elements_[1].renderer_id(),
                                         control_elements_[1].value(),
                                         FieldPropertiesFlags::kAutofilled);
  EXPECT_EQ(FieldPropertiesFlags::kNoFlags,
            field_data_manager->GetFieldPropertiesMask(
                FieldRendererId(control_elements_[1].renderer_id())));

  field_data_manager->ClearData();
  EXPECT_FALSE(field_data_manager->HasFieldData(id));
}

TEST_F(FieldDataManagerTest, UpdateFieldDataMapWithNullValue) {
  const scoped_refptr<FieldDataManager> field_data_manager =
      base::MakeRefCounted<FieldDataManager>();
  field_data_manager->UpdateFieldDataMapWithNullValue(
      control_elements_[0].renderer_id(), FieldPropertiesFlags::kUserTyped);
  const FieldRendererId id(control_elements_[0].renderer_id());
  EXPECT_TRUE(field_data_manager->HasFieldData(id));
  EXPECT_EQ(std::u16string(), field_data_manager->GetUserInput(id));
  EXPECT_EQ(FieldPropertiesFlags::kUserTyped,
            field_data_manager->GetFieldPropertiesMask(id));

  field_data_manager->UpdateFieldDataMapWithNullValue(
      control_elements_[0].renderer_id(), FieldPropertiesFlags::kAutofilled);
  EXPECT_EQ(std::u16string(), field_data_manager->GetUserInput(id));
  FieldPropertiesMask mask =
      FieldPropertiesFlags::kUserTyped | FieldPropertiesFlags::kAutofilled;
  EXPECT_EQ(mask, field_data_manager->GetFieldPropertiesMask(id));

  field_data_manager->UpdateFieldDataMap(control_elements_[0].renderer_id(),
                                         control_elements_[0].value(),
                                         FieldPropertiesFlags::kAutofilled);
  EXPECT_EQ(u"first", field_data_manager->GetUserInput(id));
}

TEST_F(FieldDataManagerTest, FindMatchedValue) {
  const scoped_refptr<FieldDataManager> field_data_manager =
      base::MakeRefCounted<FieldDataManager>();
  field_data_manager->UpdateFieldDataMap(control_elements_[0].renderer_id(),
                                         control_elements_[0].value(),
                                         FieldPropertiesFlags::kUserTyped);
  EXPECT_TRUE(field_data_manager->FindMatchedValue(u"first_element"));
  EXPECT_FALSE(field_data_manager->FindMatchedValue(u"second_element"));
}

}  // namespace autofill
