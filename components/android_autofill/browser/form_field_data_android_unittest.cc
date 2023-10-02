// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/android_autofill/browser/form_field_data_android.h"
#include "components/android_autofill/browser/form_data_android.h"

#include <memory>

#include "base/test/bind.h"
#include "components/android_autofill/browser/android_autofill_bridge_factory.h"
#include "components/android_autofill/browser/mock_form_field_data_android_bridge.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

// Registers a testing factory for `FormFieldDataAndroidBridge` that creates
// a mocked bridge and always writes the pointer of the last created bridge
// into `last_bridge` if `last_bridge` is not null.
void EnableTestingFactoryAndSaveLastBridge(
    MockFormFieldDataAndroidBridge** last_bridge) {
  AndroidAutofillBridgeFactory::GetInstance()
      .SetFormFieldDataAndroidTestingFactory(base::BindLambdaForTesting(
          [last_bridge]() -> std::unique_ptr<FormFieldDataAndroidBridge> {
            auto bridge = std::make_unique<MockFormFieldDataAndroidBridge>();
            if (last_bridge) {
              *last_bridge = bridge.get();
            }
            return bridge;
          }));
}

void EnableTestingFactory() {
  return EnableTestingFactoryAndSaveLastBridge(nullptr);
}

FormFieldData CreateTestField() {
  FormFieldData f;
  f.name = u"SomeName";
  f.name_attribute = f.name;
  f.id_attribute = u"some_id";
  f.form_control_type = FormControlType::kInputText;
  f.check_status = FormFieldData::CheckStatus::kChecked;
  return f;
}

}  // namespace

// Tests that updating the autofill types calls the Java bridge.
TEST(FormFieldDataAndroidTest, UpdateAutoFillTypes) {
  MockFormFieldDataAndroidBridge* bridge = nullptr;
  EnableTestingFactoryAndSaveLastBridge(&bridge);

  FormFieldData field;
  FormFieldDataAndroid field_android(&field);
  ASSERT_TRUE(bridge);
  EXPECT_CALL(*bridge, UpdateFieldTypes);
  field_android.UpdateAutofillTypes(FormFieldDataAndroid::FieldTypes());
}

// Tests that updating the field value calls the Java bridge and also updates
// the underlying `FormFieldData` object.
TEST(FormFieldDataAndroidTest, OnFormFieldDidChange) {
  constexpr std::u16string_view kSampleValue = u"SomeValue";

  MockFormFieldDataAndroidBridge* bridge = nullptr;
  EnableTestingFactoryAndSaveLastBridge(&bridge);

  FormFieldData field;
  field.is_autofilled = true;
  FormFieldDataAndroid field_android(&field);
  ASSERT_TRUE(bridge);
  EXPECT_CALL(*bridge, UpdateValue(kSampleValue));
  field_android.OnFormFieldDidChange(kSampleValue);
  EXPECT_FALSE(field.is_autofilled);
  EXPECT_EQ(field.value, kSampleValue);
}

// Tests that updating the field visibility calls the Java bridge and also
// updates the underlying `FormFieldData` object.
TEST(FormFieldDataAndroidTest, OnFormFieldVisibilityDidChange) {
  MockFormFieldDataAndroidBridge* bridge = nullptr;
  EnableTestingFactoryAndSaveLastBridge(&bridge);

  FormFieldData field;
  field.is_focusable = false;
  field.role = FormFieldData::RoleAttribute::kOther;
  EXPECT_FALSE(field.IsFocusable());

  FormFieldDataAndroid field_android(&field);
  FormFieldData field_copy = field;
  ASSERT_TRUE(bridge);

  // A field with `is_focusable=true` and a non-presentation role is focusable
  // in Autofill terms and therefore visible in Android Autofill terms.
  EXPECT_CALL(*bridge, UpdateVisible(true));
  field_copy.is_focusable = true;
  field_android.OnFormFieldVisibilityDidChange(field_copy);
  EXPECT_TRUE(FormFieldData::DeepEqual(field, field_copy));

  // A field with a presentation role is not focusable in Autofill terms.
  EXPECT_CALL(*bridge, UpdateVisible(false));
  field_copy.role = FormFieldData::RoleAttribute::kPresentation;
  field_android.OnFormFieldVisibilityDidChange(field_copy);
  EXPECT_TRUE(FormFieldData::DeepEqual(field, field_copy));
}

// Tests that field similarity checks include name, name_attribute, id_attribute
// and form control type.
TEST(FormFieldDataAndroidTest, SimilarFieldsAs) {
  EnableTestingFactory();
  FormFieldData f1 = CreateTestField();
  FormFieldData f2 = CreateTestField();
  FormFieldDataAndroid af(&f1);

  // If the fields are the same, they are also similar.
  EXPECT_TRUE(af.SimilarFieldAs(f2));

  // If names differ, they are not similar.
  f2.name = f1.name + u"x";
  EXPECT_FALSE(af.SimilarFieldAs(f2));

  // If name attributes differ, they are not similar.
  f2 = f1;
  f2.name_attribute = f1.name_attribute + u"x";
  EXPECT_FALSE(af.SimilarFieldAs(f2));

  // If id attributes differ, they are not similar.
  f2 = f1;
  f2.id_attribute = f1.id_attribute + u"x";
  EXPECT_FALSE(af.SimilarFieldAs(f2));

  // If form control types differ, they are not similar.
  f2 = f1;
  f2.form_control_type = FormControlType::kInputPassword;
  EXPECT_FALSE(af.SimilarFieldAs(f2));

  // If global ids differ, they are not similar.
  f2 = f1;
  f2.unique_renderer_id = FieldRendererId(f1.unique_renderer_id.value() + 1);
  EXPECT_FALSE(af.SimilarFieldAs(f2));
}

// Tests that field similarity checks whether a field is checkable, but not
// whether it is checked.
TEST(FormFieldDataAndroidTest, SimilarFieldsAs_Checkable) {
  EnableTestingFactory();
  FormFieldData f1 = CreateTestField();
  FormFieldData f2 = CreateTestField();
  f1.check_status = FormFieldData::CheckStatus::kCheckableButUnchecked;
  FormFieldDataAndroid af(&f1);

  // If they are both checkable, they are similar (even if one is checked and
  // the other is not).
  f2.check_status = FormFieldData::CheckStatus::kChecked;
  EXPECT_TRUE(af.SimilarFieldAs(f2));

  f2.check_status = FormFieldData::CheckStatus::kNotCheckable;
  EXPECT_FALSE(af.SimilarFieldAs(f2));
}

// Tests that field labels are similar if they have the same value or were
// inferred from the same source and that source is not a label tag.
TEST(FormFieldDataAndroidTest, SimilarFieldsAs_Labels) {
  EnableTestingFactory();
  FormFieldData f1 = CreateTestField();
  FormFieldData f2 = CreateTestField();
  FormFieldDataAndroid af(&f1);

  f1.label = u"SomeLabel";
  f1.label_source = FormFieldData::LabelSource::kTdTag;
  f2.label = f1.label;
  f2.label_source = FormFieldData::LabelSource::kAriaLabel;

  EXPECT_TRUE(af.SimilarFieldAs(f2));

  // Not similar because both label text and label source differ.
  f2.label = f1.label + u"x";
  EXPECT_FALSE(af.SimilarFieldAs(f2));

  // Similar because the label source are equal not not a label tag.
  f2.label_source = f1.label_source;
  EXPECT_TRUE(af.SimilarFieldAs(f2));

  // Not similar because the labels differ and the label sources are label tags.
  f1.label_source = FormFieldData::LabelSource::kLabelTag;
  f2.label_source = FormFieldData::LabelSource::kLabelTag;
  EXPECT_FALSE(af.SimilarFieldAs(f2));
}

}  // namespace autofill
