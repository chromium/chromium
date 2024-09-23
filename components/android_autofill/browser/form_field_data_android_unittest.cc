// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/android_autofill/browser/form_field_data_android.h"
#include "components/android_autofill/browser/form_data_android.h"

#include <memory>

#include "base/test/bind.h"
#include "components/android_autofill/browser/android_autofill_bridge_factory.h"
#include "components/android_autofill/browser/mock_form_field_data_android_bridge.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

FormFieldData CreateTestField() {
  FormFieldData f;
  f.set_name(u"SomeName");
  f.set_name_attribute(f.name());
  f.set_id_attribute(u"some_id");
  f.set_form_control_type(FormControlType::kInputText);
  f.set_check_status(FormFieldData::CheckStatus::kChecked);
  return f;
}

class FormFieldDataAndroidTest : public ::testing::Test {
 public:
  FormFieldDataAndroidTest() = default;
  ~FormFieldDataAndroidTest() override = default;

  void SetUp() override {
    AndroidAutofillBridgeFactory::GetInstance()
        .SetFormFieldDataAndroidTestingFactory(base::BindLambdaForTesting(
            [this]() -> std::unique_ptr<FormFieldDataAndroidBridge> {
              auto bridge = std::make_unique<MockFormFieldDataAndroidBridge>();
              last_bridge_ = bridge.get();
              return bridge;
            }));
  }

  void TearDown() override {
    last_bridge_ = nullptr;
    AndroidAutofillBridgeFactory::GetInstance()
        .SetFormFieldDataAndroidTestingFactory({});
  }

 protected:
  MockFormFieldDataAndroidBridge& bridge() { return *last_bridge_; }

 private:
  raw_ptr<MockFormFieldDataAndroidBridge> last_bridge_ = nullptr;
};

// Tests that the equality operator of FieldTypes requires that all FieldTypes
// members match the AutofillType it is compared with.
TEST_F(FormFieldDataAndroidTest, FieldTypesEquality) {
  using FieldTypes = FormFieldDataAndroid::FieldTypes;
  const AutofillType kUsername(FieldType::USERNAME);
  const AutofillType kName(FieldType::NAME_FIRST);

  const FieldTypes mixed_types(/*heuristic_type=*/kUsername,
                               /*server_type=*/kName,
                               /*server_type=*/kName,
                               /*server_predictions=*/{kName});
  const FieldTypes same_types(/*heuristic_type=*/kUsername,
                              /*server_type=*/kUsername,
                              /*server_type=*/kUsername,
                              /*server_predictions=*/{kUsername});
  EXPECT_NE(mixed_types, kUsername);
  EXPECT_NE(mixed_types, kName);
  EXPECT_NE(same_types, kName);
  EXPECT_EQ(same_types, kUsername);
}

// Tests that updating the autofill types calls the Java bridge.
TEST_F(FormFieldDataAndroidTest, UpdateAutofillTypes) {
  FormFieldData field;
  FormFieldDataAndroid field_android(&field);
  EXPECT_CALL(bridge(), UpdateFieldTypes);
  field_android.UpdateAutofillTypes(FormFieldDataAndroid::FieldTypes());
}

// Tests that calling `UpdateAutofillTypes` updates the AutofillTypes member.
TEST_F(FormFieldDataAndroidTest, UpdateAutofillTypesUpdatesFieldTypes) {
  const AutofillType kName(FieldType::NAME_FIRST);

  FormFieldData field;
  FormFieldDataAndroid field_android(&field);
  EXPECT_NE(field_android.field_types(), kName);
  field_android.UpdateAutofillTypes(FormFieldDataAndroid::FieldTypes(kName));
  EXPECT_EQ(field_android.field_types(), kName);
}

// Tests that updating the field value calls the Java bridge and also updates
// the underlying `FormFieldData` object.
TEST_F(FormFieldDataAndroidTest, OnFormFieldDidChange) {
  constexpr std::u16string_view kSampleValue = u"SomeValue";

  FormFieldData field;
  field.set_is_autofilled(true);
  FormFieldDataAndroid field_android(&field);
  EXPECT_CALL(bridge(), UpdateValue(kSampleValue));
  field_android.OnFormFieldDidChange(kSampleValue);
  EXPECT_FALSE(field.is_autofilled());
  EXPECT_EQ(field.value(), kSampleValue);
}

// Tests that updating the field visibility calls the Java bridge and also
// updates the underlying `FormFieldData` object.
TEST_F(FormFieldDataAndroidTest, OnFormFieldVisibilityDidChange) {
  FormFieldData field;
  field.set_is_focusable(false);
  field.set_role(FormFieldData::RoleAttribute::kOther);
  EXPECT_FALSE(field.IsFocusable());

  FormFieldDataAndroid field_android(&field);
  FormFieldData field_copy = field;

  // A field with `is_focusable=true` and a non-presentation role is focusable
  // in Autofill terms and therefore visible in Android Autofill terms.
  EXPECT_CALL(bridge(), UpdateVisible(true));
  field_copy.set_is_focusable(true);
  field_android.OnFormFieldVisibilityDidChange(field_copy);
  EXPECT_TRUE(FormFieldData::DeepEqual(field, field_copy));

  // A field with a presentation role is not focusable in Autofill terms.
  EXPECT_CALL(bridge(), UpdateVisible(false));
  field_copy.set_role(FormFieldData::RoleAttribute::kPresentation);
  field_android.OnFormFieldVisibilityDidChange(field_copy);
  EXPECT_TRUE(FormFieldData::DeepEqual(field, field_copy));
}

// Tests that field similarity checks include name, name_attribute, id_attribute
// and form control type.
TEST_F(FormFieldDataAndroidTest, SimilarFieldsAs) {
  FormFieldData f1 = CreateTestField();
  FormFieldData f2 = CreateTestField();
  FormFieldDataAndroid af(&f1);

  // If the fields are the same, they are also similar.
  EXPECT_TRUE(af.SimilarFieldAs(f2));

  // If names differ, they are not similar.
  f2.set_name(f1.name() + u"x");
  EXPECT_FALSE(af.SimilarFieldAs(f2));

  // If name attributes differ, they are not similar.
  f2 = f1;
  f2.set_name_attribute(f1.name_attribute() + u"x");
  EXPECT_FALSE(af.SimilarFieldAs(f2));

  // If id attributes differ, they are not similar.
  f2 = f1;
  f2.set_id_attribute(f1.id_attribute() + u"x");
  EXPECT_FALSE(af.SimilarFieldAs(f2));

  // If form control types differ, they are not similar.
  f2 = f1;
  f2.set_form_control_type(FormControlType::kInputPassword);
  EXPECT_FALSE(af.SimilarFieldAs(f2));

  // If global ids differ, they are not similar.
  f2 = f1;
  f2.set_renderer_id(FieldRendererId(f1.renderer_id().value() + 1));
  EXPECT_FALSE(af.SimilarFieldAs(f2));
}

// Tests that field similarity checks whether a field is checkable, but not
// whether it is checked.
TEST_F(FormFieldDataAndroidTest, SimilarFieldsAs_Checkable) {
  FormFieldData f1 = CreateTestField();
  FormFieldData f2 = CreateTestField();
  f1.set_check_status(FormFieldData::CheckStatus::kCheckableButUnchecked);
  FormFieldDataAndroid af(&f1);

  // If they are both checkable, they are similar (even if one is checked and
  // the other is not).
  f2.set_check_status(FormFieldData::CheckStatus::kChecked);
  EXPECT_TRUE(af.SimilarFieldAs(f2));

  f2.set_check_status(FormFieldData::CheckStatus::kNotCheckable);
  EXPECT_FALSE(af.SimilarFieldAs(f2));
}

// Tests that field labels are similar if they have the same value or were
// inferred from the same source and that source is not a label tag.
TEST_F(FormFieldDataAndroidTest, SimilarFieldsAs_Labels) {
  FormFieldData f1 = CreateTestField();
  FormFieldData f2 = CreateTestField();
  FormFieldDataAndroid af(&f1);

  f1.set_label(u"SomeLabel");
  f1.set_label_source(FormFieldData::LabelSource::kTdTag);
  f2.set_label(f1.label());
  f2.set_label_source(FormFieldData::LabelSource::kAriaLabel);

  EXPECT_TRUE(af.SimilarFieldAs(f2));

  // Not similar because both label text and label source differ.
  f2.set_label(f1.label() + u"x");
  EXPECT_FALSE(af.SimilarFieldAs(f2));

  // Similar because the label source are equal not not a label tag.
  f2.set_label_source(f1.label_source());
  EXPECT_TRUE(af.SimilarFieldAs(f2));

  // Not similar because the labels differ and the label sources are label tags.
  f1.set_label_source(FormFieldData::LabelSource::kLabelTag);
  f2.set_label_source(FormFieldData::LabelSource::kLabelTag);
  EXPECT_FALSE(af.SimilarFieldAs(f2));
}

}  // namespace
}  // namespace autofill
