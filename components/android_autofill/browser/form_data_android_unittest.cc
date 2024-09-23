// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/android_autofill/browser/form_data_android.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/test/bind.h"
#include "base/types/cxx23_to_underlying.h"
#include "components/android_autofill/browser/android_autofill_bridge_factory.h"
#include "components/android_autofill/browser/form_field_data_android.h"
#include "components/android_autofill/browser/mock_form_data_android_bridge.h"
#include "components/android_autofill/browser/mock_form_field_data_android_bridge.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using ::autofill::test::DeepEqualsFormData;
using ::testing::_;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::MockFunction;
using ::testing::Pointwise;
using ::testing::SizeIs;

constexpr SessionId kSampleSessionId(123);

MATCHER(SimilarFieldAs, "") {
  // `std::get<0>(arg)` is a `std::unique_ptr<FormFieldDataAndroid>`, while
  // `std::get<1>(arg)` is a `FormFieldData`.
  return std::get<0>(arg) && std::get<0>(arg)->SimilarFieldAs(std::get<1>(arg));
}

FormFieldData CreateTestField(std::u16string name = u"SomeName") {
  static uint64_t renderer_id = 1;
  FormFieldData f;
  f.set_name(std::move(name));
  f.set_name_attribute(f.name());
  f.set_id_attribute(u"some_id");
  f.set_form_control_type(FormControlType::kInputText);
  f.set_check_status(FormFieldData::CheckStatus::kChecked);
  f.set_role(FormFieldData::RoleAttribute::kOther);
  f.set_is_focusable(true);
  f.set_renderer_id(FieldRendererId(renderer_id++));
  return f;
}

FormData CreateTestForm() {
  FormData f;
  f.set_name(u"FormName");
  f.set_name_attribute(f.name());
  f.set_id_attribute(u"form_id");
  f.set_url(GURL("https://foo.com"));
  f.set_action(GURL("https://bar.com"));
  f.set_renderer_id(test::MakeFormRendererId());
  return f;
}

class FormDataAndroidTest : public ::testing::Test {
 public:
  FormDataAndroidTest() = default;
  ~FormDataAndroidTest() override = default;

  void SetUp() override {
    // Registers a testing factory for `FormDataAndroidBridge` that creates a
    // mocked bridge and always writes the pointer to the last created bridge
    // into `form_bridge_`.
    AndroidAutofillBridgeFactory::GetInstance()
        .SetFormDataAndroidTestingFactory(base::BindLambdaForTesting(
            [this]() -> std::unique_ptr<FormDataAndroidBridge> {
              auto bridge = std::make_unique<MockFormDataAndroidBridge>();
              form_bridge_ = bridge.get();
              return bridge;
            }));
    // Registers a testing factory for `FormFieldDataAndroidBridge` that creates
    // a mocked bridge and appends the pointers to the bridges to
    // `field_bridges_`.
    AndroidAutofillBridgeFactory::GetInstance()
        .SetFormFieldDataAndroidTestingFactory(base::BindLambdaForTesting(
            [this]() -> std::unique_ptr<FormFieldDataAndroidBridge> {
              auto bridge = std::make_unique<MockFormFieldDataAndroidBridge>();
              field_bridges_.push_back(bridge.get());
              return bridge;
            }));
  }

  void TearDown() override {
    form_bridge_ = nullptr;
    field_bridges_.clear();
    AndroidAutofillBridgeFactory::GetInstance()
        .SetFormDataAndroidTestingFactory({});
    AndroidAutofillBridgeFactory::GetInstance()
        .SetFormFieldDataAndroidTestingFactory({});
  }

 protected:
  const std::vector<MockFormFieldDataAndroidBridge*>& field_bridges() {
    return field_bridges_;
  }
  MockFormDataAndroidBridge& form_bridge() { return *form_bridge_; }

 private:
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  std::vector<MockFormFieldDataAndroidBridge*> field_bridges_;
  raw_ptr<MockFormDataAndroidBridge> form_bridge_;
};

// Tests that `FormDataAndroid` creates a copy of its argument.
TEST_F(FormDataAndroidTest, Form) {
  FormData form = CreateTestForm();
  FormDataAndroid form_android(form, kSampleSessionId);

  EXPECT_TRUE(FormData::DeepEqual(form, form_android.form()));

  form.set_name(form.name() + u"x");
  EXPECT_FALSE(FormData::DeepEqual(form, form_android.form()));
}

// Tests that form similarity checks include name, name_attribute, id_attribute,
// url, and action.
// Similarity checks are used to determine whether a web page has modified a
// field significantly enough to warrant restarting an ongoing Autofill session,
// e.g., because their change would lead to a change in type predictions. As a
// result, this check includes attributes that the user cannot change and that
// are unlikely to have been superficial dynamic changes by Javascript on the
// website.
TEST_F(FormDataAndroidTest, SimilarFormAs) {
  FormData f = CreateTestForm();
  FormDataAndroid af(f, kSampleSessionId);

  // If forms are the same, they are similar.
  EXPECT_TRUE(af.SimilarFormAs(f));

  // If names differ, they are not similar.
  f.set_name(af.form().name() + u"x");
  EXPECT_FALSE(af.SimilarFormAs(f));

  // If name attributes differ, they are not similar.
  f = af.form();
  f.set_name_attribute(af.form().name_attribute() + u"x");
  EXPECT_FALSE(af.SimilarFormAs(f));

  // If id attributes differ, they are not similar.
  f = af.form();
  f.set_id_attribute(af.form().id_attribute() + u"x");
  EXPECT_FALSE(af.SimilarFormAs(f));

  // If urls differ, they are not similar.
  f = af.form();
  f.set_url(GURL("https://other.com"));
  EXPECT_FALSE(af.SimilarFormAs(f));

  // If actions differ, they are not similar.
  f = af.form();
  f.set_action(GURL("https://other.com"));
  EXPECT_FALSE(af.SimilarFormAs(f));

  // If their global ids differ, they are not similar.
  f = af.form();
  f.set_renderer_id(FormRendererId(f.renderer_id().value() + 1));
  EXPECT_FALSE(af.SimilarFormAs(f));
}

// Tests that form similarity checks similarity of the fields.
TEST_F(FormDataAndroidTest, SimilarFormAs_Fields) {
  FormData f = CreateTestForm();
  f.set_fields({CreateTestField()});
  FormDataAndroid af(f, kSampleSessionId);

  EXPECT_TRUE(af.SimilarFormAs(f));

  // Forms with different numbers of fields are not similar.
  f.set_fields({CreateTestField(), CreateTestField()});
  EXPECT_FALSE(af.SimilarFormAs(f));

  // Forms with similar fields are similar.
  f = af.form();
  test_api(f).field(0).set_value(f.fields().front().value() + u"x");
  EXPECT_TRUE(af.SimilarFormAs(f));

  // Forms with fields that are not similar, are not similar either.
  f = af.form();
  test_api(f).field(0).set_name(f.fields().front().name() + u"x");
  EXPECT_FALSE(af.SimilarFormAs(f));
}

TEST_F(FormDataAndroidTest, GetFieldIndex) {
  FormData f = CreateTestForm();
  f.set_fields({CreateTestField(u"name1"), CreateTestField(u"name2")});
  FormDataAndroid af(f, kSampleSessionId);

  size_t index = 100;
  EXPECT_TRUE(af.GetFieldIndex(f.fields()[1], &index));
  EXPECT_EQ(index, 1u);

  // As updates in `f` are not propagated to the Android version `af`, the
  // lookup fails.
  test_api(f).field(1).set_name(u"name3");
  EXPECT_FALSE(af.GetFieldIndex(f.fields()[1], &index));
}

// Tests that `GetSimilarFieldIndex` only checks field similarity.
TEST_F(FormDataAndroidTest, GetSimilarFieldIndex) {
  FormData f = CreateTestForm();
  f.set_fields({CreateTestField(u"name1"), CreateTestField(u"name2")});
  FormDataAndroid af(f, kSampleSessionId);

  size_t index = 100;
  // Value is not part of a field similarity check, so this field is similar to
  // af.form().fields[1].
  test_api(f).field(1).set_value(u"some value");
  EXPECT_TRUE(af.GetSimilarFieldIndex(f.fields()[1], &index));
  EXPECT_EQ(index, 1u);

  // Name is a part of the field similarity check, so there is no field similar
  // to this one.
  test_api(f).field(1).set_name(u"name3");
  EXPECT_FALSE(af.GetSimilarFieldIndex(f.fields()[1], &index));
}

// Tests that calling `OnFormFieldDidChange` propagates the changes to the
// affected field.
TEST_F(FormDataAndroidTest, OnFormFieldDidChange) {
  FormData form = CreateTestForm();
  form.set_fields({CreateTestField(), CreateTestField()});
  FormDataAndroid form_android(form, kSampleSessionId);

  ASSERT_THAT(field_bridges(), SizeIs(2));
  ASSERT_TRUE(field_bridges()[0]);
  ASSERT_TRUE(field_bridges()[1]);

  constexpr std::u16string_view kNewValue = u"SomeNewValue";
  EXPECT_CALL(*field_bridges()[0], UpdateValue).Times(0);
  EXPECT_CALL(*field_bridges()[1], UpdateValue(kNewValue));
  form_android.OnFormFieldDidChange(1, kNewValue);
  EXPECT_EQ(form_android.form().fields()[1].value(), kNewValue);
}

// Tests that the calls to update field types are propagated to the fields.
TEST_F(FormDataAndroidTest, UpdateFieldTypes) {
  FormData form = CreateTestForm();
  form.set_fields({CreateTestField(), CreateTestField()});
  FormDataAndroid form_android(form, kSampleSessionId);

  ASSERT_THAT(field_bridges(), SizeIs(2));
  ASSERT_TRUE(field_bridges()[0]);
  ASSERT_TRUE(field_bridges()[1]);

  EXPECT_CALL(*field_bridges()[0], UpdateFieldTypes);
  EXPECT_CALL(*field_bridges()[1], UpdateFieldTypes);
  form_android.UpdateFieldTypes(FormStructure(form));
}

// Tests that `UpdateFieldTypes(base::flat_map<FieldGlobalId, AutofillType))`
// - sets all types (heuristic, server, computed),
// - only calls the JNI bridge for fields whose types differ.
TEST_F(FormDataAndroidTest, UpdateFieldTypesWithExplicitType) {
  const AutofillType kUsername(FieldType::USERNAME);
  const AutofillType kPassword(FieldType::PASSWORD);

  FormData form = CreateTestForm();
  form.set_fields({CreateTestField(), CreateTestField()});
  FormDataAndroid form_android(form, kSampleSessionId);
  ASSERT_THAT(field_bridges(), SizeIs(2));

  MockFunction<void(int)> check;
  {
    InSequence s;
    EXPECT_CALL(*field_bridges()[0], UpdateFieldTypes(Eq(kUsername)));
    EXPECT_CALL(*field_bridges()[1], UpdateFieldTypes(Eq(kPassword)));
    EXPECT_CALL(check, Call(1));
    EXPECT_CALL(check, Call(2));
    EXPECT_CALL(*field_bridges()[0], UpdateFieldTypes(Eq(kPassword)));
    EXPECT_CALL(check, Call(3));
    EXPECT_CALL(*field_bridges()[0], UpdateFieldTypes(Eq(kUsername)));
  }

  // Update all the fields to new types.
  form_android.UpdateFieldTypes({{form.fields()[0].global_id(), kUsername},
                                 {form.fields()[1].global_id(), kPassword}});
  check.Call(1);

  // Update to the same type - this should not trigger calls to JNI.
  form_android.UpdateFieldTypes({{form.fields()[0].global_id(), kUsername},
                                 {form.fields()[1].global_id(), kPassword}});
  check.Call(2);

  // Update only one field.
  FieldGlobalId unknown_id = CreateTestField().global_id();
  form_android.UpdateFieldTypes(
      {{form.fields()[0].global_id(), kPassword}, {unknown_id, kUsername}});
  check.Call(3);

  // Update both, but only the first one has changes.
  form_android.UpdateFieldTypes({{form.fields()[0].global_id(), kUsername},
                                 {form.fields()[1].global_id(), kPassword}});
}

// Tests that the calls to update field types are propagated to the fields.
TEST_F(FormDataAndroidTest, UpdateFieldTypes_ChangedForm) {
  FormData form = CreateTestForm();
  form.set_fields({CreateTestField(), CreateTestField()});
  FormStructure form_structure(form);
  ASSERT_EQ(form_structure.field_count(), 2u);

  form.set_fields({CreateTestField(), form.fields()[1], form.fields()[0]});

  FormDataAndroid form_android(form, kSampleSessionId);

  ASSERT_THAT(field_bridges(), SizeIs(3));
  ASSERT_TRUE(field_bridges()[0]);
  ASSERT_TRUE(field_bridges()[1]);
  ASSERT_TRUE(field_bridges()[2]);

  EXPECT_CALL(*field_bridges()[0], UpdateFieldTypes).Times(0);
  EXPECT_CALL(*field_bridges()[1], UpdateFieldTypes);
  EXPECT_CALL(*field_bridges()[2], UpdateFieldTypes);
  form_android.UpdateFieldTypes(form_structure);
}

// Tests that calling `UpdateFieldVisibilities` propagates the visibility to the
// affected fields and returns their indices.
TEST_F(FormDataAndroidTest, UpdateFieldVisibilities) {
  FormData form = CreateTestForm();
  form.set_fields({CreateTestField(), CreateTestField(), CreateTestField()});
  test_api(form).field(0).set_role(FormFieldData::RoleAttribute::kPresentation);
  test_api(form).field(1).set_is_focusable(false);
  EXPECT_FALSE(form.fields()[0].IsFocusable());
  EXPECT_FALSE(form.fields()[1].IsFocusable());
  EXPECT_TRUE(form.fields()[2].IsFocusable());
  FormDataAndroid form_android(form, kSampleSessionId);

  ASSERT_THAT(field_bridges(), SizeIs(3));
  ASSERT_TRUE(field_bridges()[0]);
  ASSERT_TRUE(field_bridges()[1]);
  ASSERT_TRUE(field_bridges()[2]);

  // `form_android` created a copy of `form` - therefore modifying the fields
  // here does not change the values inside `form_android`.
  test_api(form).field(0).set_role(FormFieldData::RoleAttribute::kOther);
  test_api(form).field(1).set_is_focusable(true);
  EXPECT_TRUE(form.fields()[0].IsFocusable());
  EXPECT_TRUE(form.fields()[1].IsFocusable());
  EXPECT_TRUE(form.fields()[2].IsFocusable());

  EXPECT_CALL(*field_bridges()[0], UpdateVisible(true));
  EXPECT_CALL(*field_bridges()[1], UpdateVisible(true));
  EXPECT_CALL(*field_bridges()[2], UpdateVisible).Times(0);
  form_android.UpdateFieldVisibilities(form);

  EXPECT_TRUE(FormData::DeepEqual(form, form_android.form()));
}

// Tests that `GetJavaPeer` passes the correct `FormData`, `SessionId` and
// `FormFieldDataAndroid` parameters to the Java bridge.
TEST_F(FormDataAndroidTest, GetJavaPeer) {
  FormData form = CreateTestForm();
  FormDataAndroid af(form, kSampleSessionId);
  EXPECT_CALL(form_bridge(),
              GetOrCreateJavaPeer(DeepEqualsFormData(form), kSampleSessionId,
                                  Pointwise(SimilarFieldAs(), form.fields())));
  af.GetJavaPeer();
}

}  // namespace
}  // namespace autofill
