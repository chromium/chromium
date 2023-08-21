// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_autofill_history.h"

#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace autofill {

using ::testing::ElementsAre;
using ::testing::Pair;

constexpr char kExampleSite[] = "https://example.com";

class FormAutofillHistoryTest : public testing::Test {
 public:
  FieldGlobalId AddNewFieldFilling(std::string_view label,
                                   std::string_view name,
                                   std::string_view value,
                                   std::string_view type,
                                   ServerFieldType field_type,
                                   bool is_autofilled = false) {
    FormFieldData field = test::CreateTestFormField(label, name, value, type);
    field.is_autofilled = is_autofilled;
    AutofillField autofill_field(field);
    autofill_field.SetTypeTo(AutofillType(field_type));
    filled_fields_.emplace_back(field, std::move(autofill_field));
    return field.global_id();
  }
  void AddFormFilling(bool is_refill) {
    std::vector<std::pair<const FormFieldData*, const AutofillField*>> fields;
    for (const auto& [field, autofill_field] : filled_fields_) {
      fields.emplace_back(&field, &autofill_field);
    }
    form_autofill_history_.AddFormFillEntry(
        fields, url::Origin::Create(GURL(kExampleSite)), is_refill);
  }

  std::vector<std::pair<FormFieldData, AutofillField>> filled_fields_;
  FormAutofillHistory form_autofill_history_;

 private:
  test::AutofillUnitTestEnvironment autofill_test_environment_;
};

// Tests the function FormAutofillHistory::AddFormFillEntry upon a normal fill.
TEST_F(FormAutofillHistoryTest, AddFormFillEntry_NormalFill) {
  FieldGlobalId first_name_id = AddNewFieldFilling(
      "first name", "first name", "some-value", "text", NAME_FIRST);
  AddFormFilling(/*is_refill=*/false);

  EXPECT_TRUE(form_autofill_history_.HasHistory(first_name_id));
  FormAutofillHistory::FillOperation fill_operation =
      form_autofill_history_.GetLastFillingOperationForField(first_name_id);
  EXPECT_EQ(fill_operation.GetAutofillValue(first_name_id),
            std::make_pair(u"some-value", false));
  EXPECT_THAT(fill_operation.GetFieldTypeMap(),
              ElementsAre(Pair(first_name_id, NAME_FIRST)));

  form_autofill_history_.Reset();
  EXPECT_FALSE(form_autofill_history_.HasHistory(first_name_id));
}

// Tests the function FormAutofillHistory::AddFormFillEntry upon a refill.
TEST_F(FormAutofillHistoryTest, AddFormFillEntry_Refill) {
  FieldGlobalId first_name_id = AddNewFieldFilling(
      "first name", "first name", "some-first-name", "text", NAME_FIRST);
  AddFormFilling(/*is_refill=*/false);

  // Modify the first name filling to simulate a refill.
  filled_fields_[0].first.value = u"some-other-first-name";
  FieldGlobalId last_name_id =
      AddNewFieldFilling("last name", "last name", "some-other-last-name",
                         "text", NAME_LAST, /*is_autofilled=*/true);
  AddFormFilling(/*is_refill=*/true);

  EXPECT_TRUE(form_autofill_history_.HasHistory(first_name_id));
  EXPECT_TRUE(form_autofill_history_.HasHistory(last_name_id));

  FormAutofillHistory::FillOperation fill_operation =
      form_autofill_history_.GetLastFillingOperationForField(first_name_id);
  EXPECT_EQ(
      fill_operation,
      form_autofill_history_.GetLastFillingOperationForField(last_name_id));
  EXPECT_EQ(fill_operation.GetAutofillValue(first_name_id),
            std::make_pair(u"some-first-name", false));
  EXPECT_EQ(fill_operation.GetAutofillValue(last_name_id),
            std::make_pair(u"some-other-last-name", true));
  EXPECT_THAT(fill_operation.GetFieldTypeMap(),
              ElementsAre(Pair(first_name_id, NAME_FIRST),
                          Pair(last_name_id, NAME_LAST)));

  form_autofill_history_.EraseFormFillEntry(fill_operation);
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
                           "text", UNKNOWN_TYPE);
  }
  AddFormFilling(/*is_refill=*/false);
  for (FieldGlobalId& field_id : fields_id) {
    EXPECT_TRUE(form_autofill_history_.HasHistory(field_id));
  }

  // Adding an extra entry that will make the history size exceed the limit.
  filled_fields_.clear();
  FieldGlobalId extra_field_id =
      AddNewFieldFilling("extra-label", "extra-name", "", "text", UNKNOWN_TYPE);
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
                       "text", UNKNOWN_TYPE);
    AddFormFilling(/*is_refill=*/false);
    filled_fields_.clear();
  }
  EXPECT_EQ(form_autofill_history_.size(), 5u);
  // Adding  a form fill entry bigger than the current size limit.
  std::vector<FieldGlobalId> fields_id(kMaxStorableFieldFillHistory + 1);
  for (size_t i = 0; i < kMaxStorableFieldFillHistory + 1; ++i) {
    fields_id[i] =
        AddNewFieldFilling(("field-label" + base::NumberToString(i)).c_str(),
                           ("field-name" + base::NumberToString(i)).c_str(), "",
                           "text", UNKNOWN_TYPE);
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

  FieldGlobalId field_id =
      AddNewFieldFilling("label", "name", "", "text", UNKNOWN_TYPE);
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
                           "text", UNKNOWN_TYPE);
  }
  AddFormFilling(/*is_refill=*/false);
  EXPECT_TRUE(form_autofill_history_.empty());

  filled_fields_.clear();
  FieldGlobalId field_id =
      AddNewFieldFilling("label", "name", "", "text", UNKNOWN_TYPE);
  AddFormFilling(/*is_refill=*/true);
  EXPECT_EQ(form_autofill_history_.size(), 1u);
  EXPECT_TRUE(form_autofill_history_.HasHistory(field_id));
}

}  // namespace autofill
