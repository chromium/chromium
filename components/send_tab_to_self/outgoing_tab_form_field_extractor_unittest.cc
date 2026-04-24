// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/outgoing_tab_form_field_extractor.h"

#include <memory>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_manager_test_api.h"
#include "components/autofill/core/browser/foundations/mock_autofill_manager.h"
#include "components/autofill/core/browser/foundations/with_test_autofill_client_driver_manager.h"
#include "components/autofill/core/browser/test_utils/autofill_form_test_utils.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/signatures.h"
#include "components/send_tab_to_self/page_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace send_tab_to_self {

namespace {

using autofill::AutofillField;
using autofill::FormControlType;
using autofill::FormData;
using autofill::FormStructure;
using ::testing::_;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::SizeIs;
using ::testing::Test;

// TODO(crbug.com/485145029): Consider introducing
// components/send_tab_to_self/test_matchers.h since matchers are duplicated
// across files.

MATCHER_P2(MatchesAutofillSignature, form_signature, field_signature, "") {
  return testing::ExplainMatchResult(
             testing::Field(
                 "form_signature",
                 &PageContext::FormFieldAutofillSignature::form_signature,
                 form_signature),
             arg, result_listener) &&
         testing::ExplainMatchResult(
             testing::Field(
                 "field_signature",
                 &PageContext::FormFieldAutofillSignature::field_signature,
                 field_signature),
             arg, result_listener);
}

MATCHER_P5(MatchesFormField,
           id_attribute,
           name_attribute,
           form_control_type,
           value,
           autofill_signature,
           "") {
  return testing::ExplainMatchResult(
             testing::Field("id_attribute",
                            &PageContext::FormField::id_attribute,
                            id_attribute),
             arg, result_listener) &&
         testing::ExplainMatchResult(
             testing::Field("name_attribute",
                            &PageContext::FormField::name_attribute,
                            name_attribute),
             arg, result_listener) &&
         testing::ExplainMatchResult(
             testing::Field("form_control_type",
                            &PageContext::FormField::form_control_type,
                            form_control_type),
             arg, result_listener) &&
         testing::ExplainMatchResult(
             testing::Field("value", &PageContext::FormField::value, value),
             arg, result_listener) &&
         testing::ExplainMatchResult(
             testing::Field("autofill_signature",
                            &PageContext::FormField::autofill_signature,
                            autofill_signature),
             arg, result_listener);
}

class OutgoingTabFormFieldExtractorTest
    : public Test,
      public autofill::WithTestAutofillClientDriverManager<
          autofill::TestAutofillClient,
          autofill::TestAutofillDriver,
          autofill::MockAutofillManager> {
 public:
  OutgoingTabFormFieldExtractorTest() = default;

  void SetUp() override {
    InitAutofillClient();
    CreateAutofillDriver();
  }

  void TearDown() override { DestroyAutofillClient(); }

 private:
  autofill::test::AutofillUnitTestEnvironment autofill_test_environment_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(OutgoingTabFormFieldExtractorTest, ShouldExtractFields) {
  const GURL kUrl("https://www.example.com");
  auto form_structure =
      std::make_unique<FormStructure>(autofill::test::GetFormData(
          {.fields = {{.label = u"label1",
                       .name_attribute = u"name1",
                       .id_attribute = u"id1",
                       .value = u"value1",
                       .origin = url::Origin::Create(kUrl)},
                      {.label = u"label2",
                       .name_attribute = u"name2",
                       .id_attribute = u"id2",
                       .value = u"value2",
                       .origin = url::Origin::Create(kUrl)}},
           .url = kUrl.spec()}));

  // Mark fields as user edited so they are not filtered out.
  form_structure->field(0)->AddFieldModifier(autofill::FieldModifier::kUser);
  form_structure->field(1)->AddFieldModifier(
      autofill::FieldModifier::kAutofill);

  autofill::test_api(autofill_manager())
      .AddSeenFormStructure(std::move(form_structure));

  EXPECT_THAT(ExtractOutgoingTabFormFields(autofill_manager(),
                                           url::Origin::Create(kUrl))
                  .fields,
              ElementsAre(MatchesFormField(u"id1", _, _, u"value1", _),
                          MatchesFormField(u"id2", _, _, u"value2", _)));
}

TEST_F(OutgoingTabFormFieldExtractorTest, ShouldFilterUninteractedFields) {
  const GURL kUrl("https://www.example.com");
  auto form_structure =
      std::make_unique<FormStructure>(autofill::test::GetFormData(
          {.fields = {{.id_attribute = u"id1",
                       .value = u"value1",
                       .origin = url::Origin::Create(kUrl)},
                      {.id_attribute = u"id2",
                       .value = u"value2",
                       .origin = url::Origin::Create(kUrl)}},
           .url = kUrl.spec()}));

  // Only field1 is interacted with.
  form_structure->field(0)->AddFieldModifier(autofill::FieldModifier::kUser);

  autofill::test_api(autofill_manager())
      .AddSeenFormStructure(std::move(form_structure));

  EXPECT_THAT(ExtractOutgoingTabFormFields(autofill_manager(),
                                           url::Origin::Create(kUrl))
                  .fields,
              ElementsAre(MatchesFormField(u"id1", _, _, _, _)));
}

TEST_F(OutgoingTabFormFieldExtractorTest, ShouldFilterEmptyFields) {
  const GURL kUrl("https://www.example.com");
  auto form_structure =
      std::make_unique<FormStructure>(autofill::test::GetFormData(
          {.fields = {{.id_attribute = u"id1",
                       .value = u"",
                       .origin = url::Origin::Create(kUrl)},
                      {.value = u"value2",
                       .origin = url::Origin::Create(kUrl)}},
           .url = kUrl.spec()}));

  form_structure->field(0)->AddFieldModifier(autofill::FieldModifier::kUser);
  form_structure->field(1)->AddFieldModifier(autofill::FieldModifier::kUser);

  autofill::test_api(autofill_manager())
      .AddSeenFormStructure(std::move(form_structure));

  EXPECT_THAT(ExtractOutgoingTabFormFields(autofill_manager(),
                                           url::Origin::Create(kUrl))
                  .fields,
              IsEmpty());
}

TEST_F(OutgoingTabFormFieldExtractorTest, ShouldFilterPasswordFields) {
  const GURL kUrl("https://www.example.com");
  auto form_structure =
      std::make_unique<FormStructure>(autofill::test::GetFormData(
          {.fields = {{.id_attribute = u"id1",
                       .value = u"secret",
                       .form_control_type = FormControlType::kInputPassword,
                       .origin = url::Origin::Create(kUrl)}},
           .url = kUrl.spec()}));

  form_structure->field(0)->AddFieldModifier(autofill::FieldModifier::kUser);

  autofill::test_api(autofill_manager())
      .AddSeenFormStructure(std::move(form_structure));

  EXPECT_THAT(ExtractOutgoingTabFormFields(autofill_manager(),
                                           url::Origin::Create(kUrl))
                  .fields,
              IsEmpty());
}

TEST_F(OutgoingTabFormFieldExtractorTest, ShouldFilterDifferentOrigin) {
  const GURL kUrl("https://www.example.com");
  const GURL kOtherUrl("https://www.other.com");
  auto form_structure =
      std::make_unique<FormStructure>(autofill::test::GetFormData(
          {.fields = {{.id_attribute = u"id1",
                       .value = u"value1",
                       .origin = url::Origin::Create(kOtherUrl)}},
           .url = kOtherUrl.spec()}));

  form_structure->field(0)->AddFieldModifier(autofill::FieldModifier::kUser);

  autofill::test_api(autofill_manager())
      .AddSeenFormStructure(std::move(form_structure));

  // Ensure that extraction works for the correct origin.
  ASSERT_THAT(ExtractOutgoingTabFormFields(autofill_manager(),
                                           url::Origin::Create(kOtherUrl))
                  .fields,
              Not(IsEmpty()));

  // Extraction for `kUrl` should ignore forms from `kOtherUrl`.
  EXPECT_THAT(ExtractOutgoingTabFormFields(autofill_manager(),
                                           url::Origin::Create(kUrl))
                  .fields,
              IsEmpty());
}

TEST_F(OutgoingTabFormFieldExtractorTest,
       ShouldExtractOnlyFieldsWithMatchingOriginInCrossOriginForm) {
  const GURL kUrl("https://www.example.com");
  const GURL kOtherUrl("https://www.other.com");
  auto form_structure =
      std::make_unique<FormStructure>(autofill::test::GetFormData(
          {.fields = {{.id_attribute = u"id1",
                       .value = u"value1",
                       .origin = url::Origin::Create(kUrl)},
                      {.id_attribute = u"id2",
                       .value = u"value2",
                       .origin = url::Origin::Create(kOtherUrl)}},
           .url = kUrl.spec()}));

  form_structure->field(0)->AddFieldModifier(autofill::FieldModifier::kUser);
  form_structure->field(1)->AddFieldModifier(autofill::FieldModifier::kUser);

  autofill::test_api(autofill_manager())
      .AddSeenFormStructure(std::move(form_structure));

  EXPECT_THAT(ExtractOutgoingTabFormFields(autofill_manager(),
                                           url::Origin::Create(kUrl))
                  .fields,
              ElementsAre(MatchesFormField(u"id1", _, _, _, _)));
}

TEST_F(OutgoingTabFormFieldExtractorTest, ShouldExtractSignatures) {
  const GURL kUrl("https://www.example.com");
  auto form_structure =
      std::make_unique<FormStructure>(autofill::test::GetFormData(
          {.fields = {{.id_attribute = u"id1",
                       .value = u"value1",
                       .origin = url::Origin::Create(kUrl)},
                      {.id_attribute = u"id2",
                       .value = u"value2",
                       .origin = url::Origin::Create(kUrl)}},
           .url = kUrl.spec()}));

  form_structure->field(0)->AddFieldModifier(autofill::FieldModifier::kUser);
  form_structure->field(1)->AddFieldModifier(autofill::FieldModifier::kUser);

  // Hardcoded values are used to detect changes in Autofill's signature
  // calculation algorithms, to know if the protocol potentially needs to be
  // updated.
  const autofill::FormSignature expected_form_signature(3892079296185715679ULL);
  const autofill::FieldSignature expected_field_sig1(1318412689);
  const autofill::FieldSignature expected_field_sig2(1318412689);

  autofill::test_api(autofill_manager())
      .AddSeenFormStructure(std::move(form_structure));

  EXPECT_THAT(
      ExtractOutgoingTabFormFields(autofill_manager(),
                                   url::Origin::Create(kUrl))
          .fields,
      ElementsAre(
          MatchesFormField(_, _, _, _,
                           MatchesAutofillSignature(expected_form_signature,
                                                    expected_field_sig1)),
          MatchesFormField(_, _, _, _,
                           MatchesAutofillSignature(expected_form_signature,
                                                    expected_field_sig2))));
}

}  // namespace

}  // namespace send_tab_to_self
