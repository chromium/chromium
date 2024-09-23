// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_autofill_history.h"

#include <optional>
#include <string_view>
#include <vector>

#include "base/uuid.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/filling_product.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

// ID of the dummy profile used for filling in tests.
const std::string kGuid = base::Uuid::GenerateRandomV4().AsLowercaseString();

class FormAutofillHistoryTest : public testing::Test {
 public:
  FieldGlobalId AddNewFieldFilling(std::string_view label,
                                   std::string_view name,
                                   std::string_view value,
                                   FormControlType type,
                                   FieldType field_type,
                                   bool is_autofilled = false) {
    FormFieldData field = test::CreateTestFormField(label, name, value, type);
    field.set_is_autofilled(is_autofilled);
    filled_fields_.push_back(field);
    AutofillField autofill_field(field);
    autofill_field.SetTypeTo(AutofillType(field_type));
    autofill_field.set_autofill_source_profile_guid(kGuid);
    autofill_field.set_autofilled_type(std::nullopt);
    autofill_field.set_filling_product(FillingProduct::kNone);
    filled_autofill_fields_.push_back(std::move(autofill_field));
    return field.global_id();
  }
  void AddFormFilling(bool is_refill) {
    std::vector<const FormFieldData*> fields;
    std::vector<const AutofillField*> autofill_fields;
    for (const FormFieldData& field : filled_fields_) {
      fields.push_back(&field);
    }
    for (const AutofillField& autofill_field : filled_autofill_fields_) {
      autofill_fields.push_back(&autofill_field);
    }
    form_autofill_history_.AddFormFillEntry(fields, autofill_fields,
                                            FillingProduct::kNone, is_refill);
  }

  std::vector<FormFieldData> filled_fields_;
  std::vector<AutofillField> filled_autofill_fields_;
  FormAutofillHistory form_autofill_history_;

 private:
  test::AutofillUnitTestEnvironment autofill_test_environment_;
};

// Tests the function FormAutofillHistory::AddFormFillEntry upon a normal fill.
TEST_F(FormAutofillHistoryTest, AddFormFillEntry_NormalFill) {
  FieldGlobalId first_name_id =
      AddNewFieldFilling("first name", "first name", "some-value",
                         FormControlType::kInputText, NAME_FIRST);
  AddFormFilling(/*is_refill=*/false);

  ASSERT_TRUE(form_autofill_history_.HasHistory(first_name_id));
  EXPECT_EQ(
      form_autofill_history_.GetLastFillingOperationForField(first_name_id)
          .GetFieldFillingEntry(first_name_id),
      FormAutofillHistory::FieldFillingEntry(
          u"some-value", false, kGuid, /*field_autofilled_type=*/std::nullopt,
          FillingProduct::kNone));

  form_autofill_history_.Reset();
  EXPECT_FALSE(form_autofill_history_.HasHistory(first_name_id));
}

// Tests the function FormAutofillHistory::AddFormFillEntry upon a refill.
TEST_F(FormAutofillHistoryTest, AddFormFillEntry_Refill) {
  FieldGlobalId first_name_id =
      AddNewFieldFilling("first name", "first name", "some-first-name",
                         FormControlType::kInputText, NAME_FIRST);
  AddFormFilling(/*is_refill=*/false);

  // Modify the first name filling to simulate a refill.
  filled_fields_[0].set_value(u"some-other-first-name");
  FieldGlobalId last_name_id = AddNewFieldFilling(
      "last name", "last name", "some-other-last-name",
      FormControlType::kInputText, NAME_LAST, /*is_autofilled=*/true);
  AddFormFilling(/*is_refill=*/true);

  EXPECT_TRUE(form_autofill_history_.HasHistory(first_name_id));
  EXPECT_TRUE(form_autofill_history_.HasHistory(last_name_id));

  EXPECT_EQ(
      form_autofill_history_.GetLastFillingOperationForField(first_name_id),
      form_autofill_history_.GetLastFillingOperationForField(last_name_id));

  ASSERT_TRUE(form_autofill_history_.HasHistory(first_name_id));
  EXPECT_EQ(
      form_autofill_history_.GetLastFillingOperationForField(first_name_id)
          .GetFieldFillingEntry(first_name_id),
      FormAutofillHistory::FieldFillingEntry(
          u"some-first-name", false, kGuid,
          /*field_autofilled_type=*/std::nullopt, FillingProduct::kNone));

  ASSERT_TRUE(form_autofill_history_.HasHistory(last_name_id));
  EXPECT_EQ(form_autofill_history_.GetLastFillingOperationForField(last_name_id)
                .GetFieldFillingEntry(last_name_id),
            FormAutofillHistory::FieldFillingEntry(
                u"some-other-last-name", true, kGuid,
                /*field_autofilled_type=*/std::nullopt, FillingProduct::kNone));

  form_autofill_history_.EraseFormFillEntry(
      form_autofill_history_.GetLastFillingOperationForField(first_name_id));
  EXPECT_TRUE(form_autofill_history_.empty());
}

// Tests how the function FormAutofillHistory::AddFormFillEntry clears values to
// remain within the size limit.
TEST_F(FormAutofillHistoryTest, AddFormFillEntry_HistoryLimit) {
  std::vector<FieldGlobalId> fields_id(kMaxStorableFieldFillHistory);
  for (size_t i = 0; i < kMaxStorableFieldFillHistory; ++i) {
    fields_id[i] =
        AddNewFieldFilling(("field-label" + base::NumberToString(i)).c_str(),
                           ("field-name" + base::NumberToString(i)).c_str(), "",
                           FormControlType::kInputText, UNKNOWN_TYPE);
  }
  AddFormFilling(/*is_refill=*/false);
  for (FieldGlobalId& field_id : fields_id) {
    EXPECT_TRUE(form_autofill_history_.HasHistory(field_id));
  }

  // Adding an extra entry that will make the history size exceed the limit.
  filled_fields_.clear();
  filled_autofill_fields_.clear();
  FieldGlobalId extra_field_id =
      AddNewFieldFilling("extra-label", "extra-name", "",
                         FormControlType::kInputText, UNKNOWN_TYPE);
  AddFormFilling(/*is_refill=*/false);

  EXPECT_EQ(form_autofill_history_.size(), 1u);
  EXPECT_TRUE(form_autofill_history_.HasHistory(extra_field_id));
  for (FieldGlobalId& field_id : fields_id) {
    EXPECT_FALSE(form_autofill_history_.HasHistory(field_id));
  }
}

// Tests how the function FormAutofillHistory::AddFormFillEntry handles a form
// entry bigger than the history size limit.
TEST_F(FormAutofillHistoryTest, AddFormFillEntry_FormBiggerThanLimit) {
  // Adding a few form fill entries.
  for (int i = 0; i < 5; ++i) {
    AddNewFieldFilling(("field-label" + base::NumberToString(i)).c_str(),
                       ("field-name" + base::NumberToString(i)).c_str(), "",
                       FormControlType::kInputText, UNKNOWN_TYPE);
    AddFormFilling(/*is_refill=*/false);
    filled_fields_.clear();
    filled_autofill_fields_.clear();
  }
  EXPECT_EQ(form_autofill_history_.size(), 5u);
  // Adding  a form fill entry bigger than the current size limit.
  std::vector<FieldGlobalId> fields_id(kMaxStorableFieldFillHistory + 1);
  for (size_t i = 0; i < kMaxStorableFieldFillHistory + 1; ++i) {
    fields_id[i] =
        AddNewFieldFilling(("field-label" + base::NumberToString(i)).c_str(),
                           ("field-name" + base::NumberToString(i)).c_str(), "",
                           FormControlType::kInputText, UNKNOWN_TYPE);
  }
  AddFormFilling(/*is_refill=*/false);
  // Expected behavior is that all entries will be dropped from the history,
  // including the last one being inserted.
  EXPECT_TRUE(form_autofill_history_.empty());
}

// Tests how the function FormAutofillHistory::AddFormFillEntry reuses space
// after adding an empty form fill entry.
TEST_F(FormAutofillHistoryTest, AddFormFillEntry_ReuseEmptyFillEntries) {
  // No fields were added to `filled_fields`, hence this form filling is empty.
  AddFormFilling(/*is_refill=*/false);
  EXPECT_EQ(form_autofill_history_.size(), 1u);

  FieldGlobalId field_id = AddNewFieldFilling(
      "label", "name", "", FormControlType::kInputText, UNKNOWN_TYPE);
  AddFormFilling(/*is_refill=*/false);
  EXPECT_EQ(form_autofill_history_.size(), 1u);
  EXPECT_TRUE(form_autofill_history_.HasHistory(field_id));
}

// Tests how the function FormAutofillHistory::AddFormFillEntry reuses space
// after adding an empty form fill entry.
TEST_F(FormAutofillHistoryTest, AddFormFillEntry_RefillOnEmptyHistory) {
  // Adding a form fill entry bigger than current size limit.
  std::vector<FieldGlobalId> fields_id(kMaxStorableFieldFillHistory + 1);
  for (size_t i = 0; i < kMaxStorableFieldFillHistory + 1; ++i) {
    fields_id[i] =
        AddNewFieldFilling(("field-label" + base::NumberToString(i)).c_str(),
                           ("field-name" + base::NumberToString(i)).c_str(), "",
                           FormControlType::kInputText, UNKNOWN_TYPE);
  }
  AddFormFilling(/*is_refill=*/false);
  EXPECT_TRUE(form_autofill_history_.empty());

  filled_fields_.clear();
  filled_autofill_fields_.clear();
  FieldGlobalId field_id = AddNewFieldFilling(
      "label", "name", "", FormControlType::kInputText, UNKNOWN_TYPE);
  AddFormFilling(/*is_refill=*/true);
  EXPECT_EQ(form_autofill_history_.size(), 1u);
  EXPECT_TRUE(form_autofill_history_.HasHistory(field_id));
}

}  // namespace

}  // namespace autofill
