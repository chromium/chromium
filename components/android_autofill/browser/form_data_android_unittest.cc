// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/android_autofill/browser/form_data_android.h"
#include "components/android_autofill/browser/form_field_data_android.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/test/bind.h"
#include "components/android_autofill/browser/android_autofill_bridge_factory.h"
#include "components/android_autofill/browser/mock_form_data_android_bridge.h"
#include "components/android_autofill/browser/mock_form_field_data_android_bridge.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

using ::autofill::test::DeepEqualsFormData;
using ::testing::_;
using ::testing::Pointwise;
using ::testing::SizeIs;

constexpr SessionId kSampleSessionId(123);

MATCHER(SimilarFieldAs, "") {
  // `std::get<0>(arg)` is a `std::unique_ptr<FormFieldDataAndroid>`, while
  // `std::get<1>(arg)` is a `FormFieldData`.
  return std::get<0>(arg) && std::get<0>(arg)->SimilarFieldAs(std::get<1>(arg));
}

// Registers a testing factory for `FormDataAndroidBridge` that creates
// a mocked bridge and always writes the pointer of the last created bridge
// into `last_bridge` if `last_bridge` is not null.
void EnableFormTestingFactoryAndSaveLastBridge(
    MockFormDataAndroidBridge** last_bridge) {
  AndroidAutofillBridgeFactory::GetInstance().SetFormDataAndroidTestingFactory(
      base::BindLambdaForTesting(
          [last_bridge]() -> std::unique_ptr<FormDataAndroidBridge> {
            auto bridge = std::make_unique<MockFormDataAndroidBridge>();
            if (last_bridge) {
              *last_bridge = bridge.get();
            }
            return bridge;
          }));
}

// Registers a testing factory for `FormFieldDataAndroidBridge` that creates
// a mocked bridge and appends the pointers of the bridges to `bridges`, if it
// is not a nullptr.
void EnableFieldTestingFactoryAndSaveBridges(
    std::vector<MockFormFieldDataAndroidBridge*>* bridges) {
  AndroidAutofillBridgeFactory::GetInstance()
      .SetFormFieldDataAndroidTestingFactory(base::BindLambdaForTesting(
          [bridges]() -> std::unique_ptr<FormFieldDataAndroidBridge> {
            auto bridge = std::make_unique<MockFormFieldDataAndroidBridge>();
            if (bridges) {
              bridges->push_back(bridge.get());
            }
            return bridge;
          }));
}

FormFieldData CreateTestField(std::u16string name = u"SomeName") {
  static uint64_t renderer_id = 1;
  FormFieldData f;
  f.name = std::move(name);
  f.name_attribute = f.name;
  f.id_attribute = u"some_id";
  f.form_control_type = FormControlType::kInputText;
  f.check_status = FormFieldData::CheckStatus::kChecked;
  f.role = FormFieldData::RoleAttribute::kOther;
  f.is_focusable = true;
  f.unique_renderer_id = FieldRendererId(renderer_id++);
  return f;
}

FormData CreateTestForm() {
  FormData f;
  f.name = u"FormName";
  f.name_attribute = f.name;
  f.id_attribute = u"form_id";
  f.url = GURL("https://foo.com");
  f.action = GURL("https://bar.com");
  f.is_action_empty = false;
  f.is_form_tag = true;
  return f;
}

}  // namespace

// Tests that `FormDataAndroid` creates a copy of its argument.
TEST(FormDataAndroidTest, Form) {
  FormData form = CreateTestForm();
  FormDataAndroid form_android(form, kSampleSessionId);

  EXPECT_TRUE(FormData::DeepEqual(form, form_android.form()));

  form.name = form.name + u"x";
  EXPECT_FALSE(FormData::DeepEqual(form, form_android.form()));
}

// Tests that form similarity checks include name, name_attribute, id_attribute,
// url, action, is_action_empty, and is_form_tag.
// Similarity checks are used to determine whether a web page has modified a
// field significantly enough to warrant restarting an ongoing Autofill session,
// e.g., because their change would lead to a change in type predictions. As a
// result, this check includes attributes that the user cannot change and that
// are unlikely to have been superficial dynamic changes by Javascript on the
// website.
TEST(FormDataAndroidTest, SimilarFormAs) {
  FormDataAndroid af(CreateTestForm(), kSampleSessionId);
  FormData f = CreateTestForm();

  // If forms are the same, they are similar.
  EXPECT_TRUE(af.SimilarFormAs(f));

  // If names differ, they are not similar.
  f.name = af.form().name + u"x";
  EXPECT_FALSE(af.SimilarFormAs(f));

  // If name attributes differ, they are not similar.
  f = af.form();
  f.name_attribute = af.form().name_attribute + u"x";
  EXPECT_FALSE(af.SimilarFormAs(f));

  // If id attributes differ, they are not similar.
  f = af.form();
  f.id_attribute = af.form().id_attribute + u"x";
  EXPECT_FALSE(af.SimilarFormAs(f));

  // If urls differ, they are not similar.
  f = af.form();
  f.url = GURL("https://other.com");
  EXPECT_FALSE(af.SimilarFormAs(f));

  // If actions differ, they are not similar.
  f = af.form();
  f.action = GURL("https://other.com");
  EXPECT_FALSE(af.SimilarFormAs(f));

  // If is_action_empty differs, they are not similar.
  f = af.form();
  f.is_action_empty = !f.is_action_empty;
  EXPECT_FALSE(af.SimilarFormAs(f));

  // If is_form_tag differs, they are not similar.
  f = af.form();
  f.is_form_tag = !f.is_form_tag;
  EXPECT_FALSE(af.SimilarFormAs(f));

  // If their global ids differ, they are not similar.
  f = af.form();
  f.unique_renderer_id = FormRendererId(f.unique_renderer_id.value() + 1);
  EXPECT_FALSE(af.SimilarFormAs(f));
}

// Tests that form similarity checks similarity of the fields.
TEST(FormDataAndroidTest, SimilarFormAs_Fields) {
  FormData f = CreateTestForm();
  f.fields = {CreateTestField()};
  FormDataAndroid af(f, kSampleSessionId);

  EXPECT_TRUE(af.SimilarFormAs(f));

  // Forms with different numbers of fields are not similar.
  f.fields = {CreateTestField(), CreateTestField()};
  EXPECT_FALSE(af.SimilarFormAs(f));

  // Forms with similar fields are similar.
  f = af.form();
  f.fields.front().value = f.fields.front().value + u"x";
  EXPECT_TRUE(af.SimilarFormAs(f));

  // Forms with fields that are not similar, are not similar either.
  f = af.form();
  f.fields.front().name += u"x";
  EXPECT_FALSE(af.SimilarFormAs(f));
}

TEST(FormDataAndroidTest, GetFieldIndex) {
  FormData f = CreateTestForm();
  f.fields = {CreateTestField(u"name1"), CreateTestField(u"name2")};
  FormDataAndroid af(f, kSampleSessionId);

  size_t index = 100;
  EXPECT_TRUE(af.GetFieldIndex(f.fields[1], &index));
  EXPECT_EQ(index, 1u);

  // As updates in `f` are not propagated to the Android version `af`, the
  // lookup fails.
  f.fields[1].name = u"name3";
  EXPECT_FALSE(af.GetFieldIndex(f.fields[1], &index));
}

// Tests that `GetSimilarFieldIndex` only checks field similarity.
TEST(FormDataAndroidTest, GetSimilarFieldIndex) {
  FormData f = CreateTestForm();
  f.fields = {CreateTestField(u"name1"), CreateTestField(u"name2")};
  FormDataAndroid af(f, kSampleSessionId);

  size_t index = 100;
  // Value is not part of a field similarity check, so this field is similar to
  // af.form().fields[1].
  f.fields[1].value = u"some value";
  EXPECT_TRUE(af.GetSimilarFieldIndex(f.fields[1], &index));
  EXPECT_EQ(index, 1u);

  // Name is a part of the field similarity check, so there is no field similar
  // to this one.
  f.fields[1].name = u"name3";
  EXPECT_FALSE(af.GetSimilarFieldIndex(f.fields[1], &index));
}

// Tests that calling `OnFormFieldDidChange` propagates the changes to the
// affected field.
TEST(FormDataAndroidTest, OnFormFieldDidChange) {
  std::vector<MockFormFieldDataAndroidBridge*> bridges;
  EnableFieldTestingFactoryAndSaveBridges(&bridges);

  FormData form = CreateTestForm();
  form.fields = {CreateTestField(), CreateTestField()};
  FormDataAndroid form_android(form, kSampleSessionId);

  ASSERT_THAT(bridges, SizeIs(2));
  ASSERT_TRUE(bridges[0]);
  ASSERT_TRUE(bridges[1]);

  constexpr std::u16string_view kNewValue = u"SomeNewValue";
  EXPECT_CALL(*bridges[0], UpdateValue).Times(0);
  EXPECT_CALL(*bridges[1], UpdateValue(kNewValue));
  form_android.OnFormFieldDidChange(1, kNewValue);
  EXPECT_EQ(form_android.form().fields[1].value, kNewValue);
}

// Tests that the calls to update field types are propagated to the fields.
TEST(FormDataAndroidTest, UpdateFieldTypes) {
  std::vector<MockFormFieldDataAndroidBridge*> bridges;
  EnableFieldTestingFactoryAndSaveBridges(&bridges);

  FormData form = CreateTestForm();
  form.fields = {CreateTestField(), CreateTestField()};
  FormDataAndroid form_android(form, kSampleSessionId);

  ASSERT_THAT(bridges, SizeIs(2));
  ASSERT_TRUE(bridges[0]);
  ASSERT_TRUE(bridges[1]);

  EXPECT_CALL(*bridges[0], UpdateFieldTypes);
  EXPECT_CALL(*bridges[1], UpdateFieldTypes);
  form_android.UpdateFieldTypes(FormStructure(form));
}

// Tests that the calls to update field types are propagated to the fields.
TEST(FormDataAndroidTest, UpdateFieldTypes_ChangedForm) {
  std::vector<MockFormFieldDataAndroidBridge*> bridges;
  EnableFieldTestingFactoryAndSaveBridges(&bridges);

  FormData form = CreateTestForm();
  form.fields = {CreateTestField(), CreateTestField()};
  FormStructure form_structure(form);
  ASSERT_EQ(form_structure.field_count(), 2u);

  form.fields.push_back(CreateTestField());
  std::swap(form.fields.front(), form.fields.back());
  FormDataAndroid form_android(form, kSampleSessionId);

  ASSERT_THAT(bridges, SizeIs(3));
  ASSERT_TRUE(bridges[0]);
  ASSERT_TRUE(bridges[1]);
  ASSERT_TRUE(bridges[2]);

  EXPECT_CALL(*bridges[0], UpdateFieldTypes).Times(0);
  EXPECT_CALL(*bridges[1], UpdateFieldTypes);
  EXPECT_CALL(*bridges[2], UpdateFieldTypes);
  form_android.UpdateFieldTypes(form_structure);
}

// Tests that calling `UpdateFieldVisibilities` propagates the visibility to the
// affected fields and returns their indices.
TEST(FormDataAndroidTest, UpdateFieldVisibilities) {
  std::vector<MockFormFieldDataAndroidBridge*> bridges;
  EnableFieldTestingFactoryAndSaveBridges(&bridges);

  FormData form = CreateTestForm();
  form.fields = {CreateTestField(), CreateTestField(), CreateTestField()};
  form.fields[0].role = FormFieldData::RoleAttribute::kPresentation;
  form.fields[1].is_focusable = false;
  EXPECT_FALSE(form.fields[0].IsFocusable());
  EXPECT_FALSE(form.fields[1].IsFocusable());
  EXPECT_TRUE(form.fields[2].IsFocusable());
  FormDataAndroid form_android(form, kSampleSessionId);

  ASSERT_THAT(bridges, SizeIs(3));
  ASSERT_TRUE(bridges[0]);
  ASSERT_TRUE(bridges[1]);
  ASSERT_TRUE(bridges[2]);

  // `form_android` created a copy of `form` - therefore modifying the fields
  // here does not change the values inside `form_android`.
  form.fields[0].role = FormFieldData::RoleAttribute::kOther;
  form.fields[1].is_focusable = true;
  EXPECT_TRUE(form.fields[0].IsFocusable());
  EXPECT_TRUE(form.fields[1].IsFocusable());
  EXPECT_TRUE(form.fields[2].IsFocusable());

  EXPECT_CALL(*bridges[0], UpdateVisible(true));
  EXPECT_CALL(*bridges[1], UpdateVisible(true));
  EXPECT_CALL(*bridges[2], UpdateVisible).Times(0);
  form_android.UpdateFieldVisibilities(form);

  EXPECT_TRUE(FormData::DeepEqual(form, form_android.form()));
}

// Tests that `GetJavaPeer` passes the correct `FormData`, `SessionId` and
// `FormFieldDataAndroid` parameters to the Java bridge.
TEST(FormDataAndroidTest, GetJavaPeer) {
  MockFormDataAndroidBridge* bridge = nullptr;
  EnableFormTestingFactoryAndSaveLastBridge(&bridge);

  FormData form = CreateTestForm();
  FormDataAndroid af(form, kSampleSessionId);
  EXPECT_CALL(*bridge,
              GetOrCreateJavaPeer(DeepEqualsFormData(form), kSampleSessionId,
                                  Pointwise(SimilarFieldAs(), form.fields)));
  af.GetJavaPeer();
}

}  // namespace autofill
