// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/crowdsourcing/autofill_crowdsourcing_encoding.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_form_test_utils.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/proto/api_v1.pb.h"
#include "components/autofill/core/browser/randomized_encoder.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/version_info/version_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/re2/src/re2/re2.h"

namespace autofill {

namespace {
using ::autofill::mojom::SubmissionIndicatorEvent;
using ::autofill::mojom::SubmissionSource;
using ::autofill::test::CreateTestFormField;
using ::base::ASCIIToUTF16;
using ::testing::Truly;
using ::version_info::GetProductNameAndVersionForUserAgent;

// Matches any protobuf `actual` whose serialization is equal to the
// string-serialization of the protobuf `expected`.
template <typename T>
auto SerializesSameAs(const T& expected) {
  std::string expected_string;
  CHECK(expected.SerializeToString(&expected_string));
  return Truly([expected_string](const auto& actual) {
    std::string actual_string;
    CHECK(actual.SerializeToString(&actual_string));
    return actual_string == expected_string;
  });
}

template <typename... Matchers>
auto ElementsSerializeSameAs(Matchers... element_matchers) {
  return ElementsAre(SerializesSameAs(element_matchers)...);
}

template <typename... Matchers>
auto UnorderedElementsSerializeSameAs(Matchers... element_matchers) {
  return UnorderedElementsAre(SerializesSameAs(element_matchers)...);
}

}  // namespace

class AutofillCrowdsourcingEncoding : public testing::Test {
 public:
  AutofillCrowdsourcingEncoding() = default;

 private:
  test::AutofillUnitTestEnvironment autofill_test_environment_;
};

TEST_F(AutofillCrowdsourcingEncoding,
       EncodeUploadRequest_SubmissionIndicatorEvents_Match) {
  // Statically assert that the mojo SubmissionIndicatorEvent enum matches the
  // corresponding entries the in proto AutofillUploadContents
  // SubmissionIndicatorEvent enum.
  static_assert(AutofillUploadContents::NONE ==
                    static_cast<int>(SubmissionIndicatorEvent::NONE),
                "NONE enumerator does not match!");
  static_assert(
      AutofillUploadContents::HTML_FORM_SUBMISSION ==
          static_cast<int>(SubmissionIndicatorEvent::HTML_FORM_SUBMISSION),
      "HTML_FORM_SUBMISSION enumerator does not match!");
  static_assert(
      AutofillUploadContents::SAME_DOCUMENT_NAVIGATION ==
          static_cast<int>(SubmissionIndicatorEvent::SAME_DOCUMENT_NAVIGATION),
      "SAME_DOCUMENT_NAVIGATION enumerator does not match!");
  static_assert(AutofillUploadContents::XHR_SUCCEEDED ==
                    static_cast<int>(SubmissionIndicatorEvent::XHR_SUCCEEDED),
                "XHR_SUCCEEDED enumerator does not match!");
  static_assert(AutofillUploadContents::FRAME_DETACHED ==
                    static_cast<int>(SubmissionIndicatorEvent::FRAME_DETACHED),
                "FRAME_DETACHED enumerator does not match!");
  static_assert(
      AutofillUploadContents::PROBABLE_FORM_SUBMISSION ==
          static_cast<int>(SubmissionIndicatorEvent::PROBABLE_FORM_SUBMISSION),
      "PROBABLE_FORM_SUBMISSION enumerator does not match!");
  static_assert(AutofillUploadContents::DOM_MUTATION_AFTER_AUTOFILL ==
                    static_cast<int>(
                        SubmissionIndicatorEvent::DOM_MUTATION_AFTER_AUTOFILL),
                "DOM_MUTATION_AFTER_AUTOFILL enumerator does not match!");
}

TEST_F(AutofillCrowdsourcingEncoding,
       EncodeUploadRequest_WithMatchingValidities) {
  ////////////////
  // Setup
  ////////////////
  std::unique_ptr<FormStructure> form_structure;
  std::vector<FieldTypeSet> possible_field_types;
  std::vector<FieldTypeValidityStatesMap> possible_field_types_validities;
  FormData form;
  form.url = GURL("http://www.foo.com/");
  form.is_form_tag = true;

  FormFieldData field;
  field.form_control_type = FormControlType::kInputText;

  field.label = u"First Name";
  field.name = u"firstname";
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_FIRST},
      {AutofillProfile::ValidityState::kUnvalidated});
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Last Name";
  field.name = u"lastname";
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_LAST},
      {AutofillProfile::ValidityState::kUnvalidated});
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Email";
  field.name = u"email";
  field.form_control_type = FormControlType::kInputEmail;
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {EMAIL_ADDRESS},
      {AutofillProfile::ValidityState::kInvalid});
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Phone";
  field.name = u"phone";
  field.form_control_type = FormControlType::kInputNumber;
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities,
      {PHONE_HOME_WHOLE_NUMBER}, {AutofillProfile::ValidityState::kEmpty});
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Country";
  field.name = u"country";
  field.form_control_type = FormControlType::kSelectOne;
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities,
      {ADDRESS_HOME_COUNTRY}, {AutofillProfile::ValidityState::kValid});
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  // Add checkable field.
  FormFieldData checkable_field;
  checkable_field.check_status =
      FormFieldData::CheckStatus::kCheckableButUnchecked;
  checkable_field.label = u"Checkable1";
  checkable_field.name = u"Checkable1";
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities,
      {ADDRESS_HOME_COUNTRY}, {AutofillProfile::ValidityState::kValid});
  checkable_field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(checkable_field);

  form_structure = std::make_unique<FormStructure>(form);
  for (auto& fs_field : *form_structure) {
    fs_field->host_form_signature = form_structure->form_signature();
  }

  ASSERT_EQ(form_structure->field_count(), possible_field_types.size());
  ASSERT_EQ(form_structure->field_count(),
            possible_field_types_validities.size());
  for (size_t i = 0; i < form_structure->field_count(); ++i) {
    form_structure->field(i)->set_possible_types(possible_field_types[i]);
    form_structure->field(i)->set_possible_types_validities(
        possible_field_types_validities[i]);
  }

  FieldTypeSet available_field_types;
  available_field_types.insert(NAME_FIRST);
  available_field_types.insert(NAME_LAST);
  available_field_types.insert(ADDRESS_HOME_LINE1);
  available_field_types.insert(ADDRESS_HOME_LINE2);
  available_field_types.insert(ADDRESS_HOME_COUNTRY);
  available_field_types.insert(EMAIL_ADDRESS);
  available_field_types.insert(PHONE_HOME_WHOLE_NUMBER);

  // Prepare the expected proto string.
  AutofillUploadContents upload;
  upload.set_submission(true);
  upload.set_client_version(
      std::string(GetProductNameAndVersionForUserAgent()));
  upload.set_form_signature(form_structure->form_signature().value());
  upload.set_autofill_used(false);
  upload.set_data_present("1442000308");
  upload.set_submission_event(
      AutofillUploadContents_SubmissionIndicatorEvent_NONE);
  upload.set_has_form_tag(true);

  test::FillUploadField(upload.add_field(), 3763331450U, 3U, 0);
  test::FillUploadField(upload.add_field(), 3494530716U, 5U, 0);
  test::FillUploadField(upload.add_field(), 1029417091U, 9U, 3);
  test::FillUploadField(upload.add_field(), 466116101U, 14U, 1);
  test::FillUploadField(upload.add_field(), 2799270304U, 36U, 2);

  ////////////////
  // Verification
  ////////////////
  std::string expected_upload_string;
  EXPECT_THAT(EncodeUploadRequest(*form_structure, available_field_types, false,
                                  std::string(), true),
              ElementsSerializeSameAs(upload));

  upload.set_autofill_used(true);
  EXPECT_THAT(EncodeUploadRequest(*form_structure, available_field_types, true,
                                  std::string(), true),
              ElementsSerializeSameAs(upload));

  ////////////////
  // Setup
  ////////////////
  // Add 2 address fields - this should be still a valid form.
  for (size_t i = 0; i < 2; ++i) {
    field.label = u"Address";
    field.name = u"address";
    field.form_control_type = FormControlType::kInputText;
    field.unique_renderer_id = test::MakeFieldRendererId();
    form.fields.push_back(field);
    test::InitializePossibleTypesAndValidities(
        possible_field_types, possible_field_types_validities,
        {ADDRESS_HOME_LINE1, ADDRESS_HOME_LINE2},
        {AutofillProfile::ValidityState::kValid,
         AutofillProfile::ValidityState::kValid});
  }

  form_structure = std::make_unique<FormStructure>(form);
  for (auto& fs_field : *form_structure) {
    fs_field->host_form_signature = form_structure->form_signature();
  }

  ASSERT_EQ(form_structure->field_count(), possible_field_types.size());
  ASSERT_EQ(form_structure->field_count(),
            possible_field_types_validities.size());
  for (size_t i = 0; i < form_structure->field_count(); ++i) {
    form_structure->field(i)->set_possible_types(possible_field_types[i]);
    form_structure->field(i)->set_possible_types_validities(
        possible_field_types_validities[i]);
  }

  // Adjust the expected proto string.
  upload.set_form_signature(form_structure->form_signature().value());
  upload.set_autofill_used(false);
  // Create an additional 2 fields (total of 7).  Put the appropriate autofill
  // type on the different address fields.
  test::FillUploadField(upload.add_field(), 509334676U, {30U, 31U}, {2, 2});
  test::FillUploadField(upload.add_field(), 509334676U, {30U, 31U}, {2, 2});

  EXPECT_THAT(EncodeUploadRequest(*form_structure, available_field_types, false,
                                  std::string(), true),
              ElementsSerializeSameAs(upload));
}

TEST_F(AutofillCrowdsourcingEncoding,
       EncodeUploadRequest_WithNonMatchingValidities) {
  ////////////////
  // Setup
  ////////////////
  std::unique_ptr<FormStructure> form_structure;
  std::vector<FieldTypeSet> possible_field_types;
  std::vector<FieldTypeValidityStatesMap> possible_field_types_validities;
  FormData form;
  form.url = GURL("http://www.foo.com/");

  FormFieldData field;
  field.form_control_type = FormControlType::kInputText;

  field.label = u"First Name";
  field.name = u"firstname";
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_FIRST},
      {AutofillProfile::ValidityState::kUnvalidated});
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Last Name";
  field.name = u"lastname";
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_LAST},
      {AutofillProfile::ValidityState::kUnvalidated});
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Email";
  field.name = u"email";
  field.form_control_type = FormControlType::kInputEmail;
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {EMAIL_ADDRESS},
      {AutofillProfile::ValidityState::kInvalid});
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Phone";
  field.name = u"phone";
  field.form_control_type = FormControlType::kInputNumber;
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities,
      {PHONE_HOME_WHOLE_NUMBER}, {AutofillProfile::ValidityState::kEmpty});
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Country";
  field.name = u"country";
  field.form_control_type = FormControlType::kSelectOne;
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities,
      {ADDRESS_HOME_COUNTRY}, {AutofillProfile::ValidityState::kValid});
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  // Add checkable field.
  FormFieldData checkable_field;
  checkable_field.check_status =
      FormFieldData::CheckStatus::kCheckableButUnchecked;
  checkable_field.label = u"Checkable1";
  checkable_field.name = u"Checkable1";
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities,
      {ADDRESS_HOME_COUNTRY}, {AutofillProfile::ValidityState::kValid});
  checkable_field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(checkable_field);

  form_structure = std::make_unique<FormStructure>(form);
  for (auto& fs_field : *form_structure) {
    fs_field->host_form_signature = form_structure->form_signature();
  }

  ASSERT_EQ(form_structure->field_count(), possible_field_types.size());
  ASSERT_EQ(form_structure->field_count(),
            possible_field_types_validities.size());
  for (size_t i = 0; i < form_structure->field_count(); ++i) {
    form_structure->field(i)->set_possible_types(possible_field_types[i]);
    form_structure->field(i)->set_possible_types_validities(
        possible_field_types_validities[i]);
  }

  FieldTypeSet available_field_types;
  available_field_types.insert(NAME_FIRST);
  available_field_types.insert(NAME_LAST);
  available_field_types.insert(ADDRESS_HOME_LINE1);
  available_field_types.insert(ADDRESS_HOME_LINE2);
  available_field_types.insert(ADDRESS_HOME_COUNTRY);
  available_field_types.insert(EMAIL_ADDRESS);
  available_field_types.insert(PHONE_HOME_WHOLE_NUMBER);

  // Prepare the expected proto string.
  AutofillUploadContents upload;
  upload.set_submission(true);
  upload.set_client_version(
      std::string(GetProductNameAndVersionForUserAgent()));
  upload.set_form_signature(form_structure->form_signature().value());
  upload.set_autofill_used(false);
  upload.set_data_present("1442000308");

  test::FillUploadField(upload.add_field(), 3763331450U, 3U, 0);
  test::FillUploadField(upload.add_field(), 3494530716U, 5U, 0);
  test::FillUploadField(upload.add_field(), 1029417091U, 9U, 3);
  test::FillUploadField(upload.add_field(), 466116101U, 14U, 1);
  test::FillUploadField(upload.add_field(), 2799270304U, 36U,
                        1);  // Non-matching validities

  EXPECT_THAT(EncodeUploadRequest(*form_structure, available_field_types, false,
                                  std::string(), true),
              ElementsAre(Not(SerializesSameAs(upload))));
}

TEST_F(AutofillCrowdsourcingEncoding,
       EncodeUploadRequest_WithMultipleValidities) {
  ////////////////
  // Setup
  ////////////////
  std::unique_ptr<FormStructure> form_structure;
  std::vector<FieldTypeSet> possible_field_types;
  std::vector<FieldTypeValidityStatesMap> possible_field_types_validities;
  FormData form;
  form.url = GURL("http://www.foo.com/");
  form.is_form_tag = true;

  FormFieldData field;
  field.form_control_type = FormControlType::kInputText;

  field.label = u"First Name";
  field.name = u"firstname";
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_FIRST},
      {AutofillProfile::ValidityState::kUnvalidated,
       AutofillProfile::ValidityState::kValid});
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Last Name";
  field.name = u"lastname";
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_LAST},
      {AutofillProfile::ValidityState::kUnvalidated,
       AutofillProfile::ValidityState::kValid});
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Email";
  field.name = u"email";
  field.form_control_type = FormControlType::kInputEmail;
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {EMAIL_ADDRESS},
      {AutofillProfile::ValidityState::kInvalid,
       AutofillProfile::ValidityState::kValid});
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Phone";
  field.name = u"phone";
  field.form_control_type = FormControlType::kInputNumber;
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities,
      {PHONE_HOME_WHOLE_NUMBER},
      {AutofillProfile::ValidityState::kEmpty,
       AutofillProfile::ValidityState::kValid});
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Country";
  field.name = u"country";
  field.form_control_type = FormControlType::kSelectOne;
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities,
      {ADDRESS_HOME_COUNTRY},
      {AutofillProfile::ValidityState::kValid,
       AutofillProfile::ValidityState::kValid});
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  // Add checkable field.
  FormFieldData checkable_field;
  checkable_field.check_status =
      FormFieldData::CheckStatus::kCheckableButUnchecked;
  checkable_field.label = u"Checkable1";
  checkable_field.name = u"Checkable1";
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities,
      {ADDRESS_HOME_COUNTRY},
      {AutofillProfile::ValidityState::kValid,
       AutofillProfile::ValidityState::kValid});
  checkable_field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(checkable_field);

  form_structure = std::make_unique<FormStructure>(form);
  for (auto& fs_field : *form_structure) {
    fs_field->host_form_signature = form_structure->form_signature();
  }

  ASSERT_EQ(form_structure->field_count(), possible_field_types.size());
  ASSERT_EQ(form_structure->field_count(),
            possible_field_types_validities.size());
  for (size_t i = 0; i < form_structure->field_count(); ++i) {
    form_structure->field(i)->set_possible_types(possible_field_types[i]);
    form_structure->field(i)->set_possible_types_validities(
        possible_field_types_validities[i]);
  }

  FieldTypeSet available_field_types;
  available_field_types.insert(NAME_FIRST);
  available_field_types.insert(NAME_LAST);
  available_field_types.insert(ADDRESS_HOME_LINE1);
  available_field_types.insert(ADDRESS_HOME_LINE2);
  available_field_types.insert(ADDRESS_HOME_COUNTRY);
  available_field_types.insert(EMAIL_ADDRESS);
  available_field_types.insert(PHONE_HOME_WHOLE_NUMBER);

  // Prepare the expected proto string.
  AutofillUploadContents upload;
  upload.set_submission(true);
  upload.set_client_version(
      std::string(GetProductNameAndVersionForUserAgent()));
  upload.set_form_signature(form_structure->form_signature().value());
  upload.set_autofill_used(false);
  upload.set_data_present("1442000308");
  upload.set_submission_event(
      AutofillUploadContents_SubmissionIndicatorEvent_NONE);
  upload.set_has_form_tag(true);

  test::FillUploadField(upload.add_field(), 3763331450U, 3U, {0, 2});
  test::FillUploadField(upload.add_field(), 3494530716U, 5U, {0, 2});
  test::FillUploadField(upload.add_field(), 1029417091U, 9U, {3, 2});
  test::FillUploadField(upload.add_field(), 466116101U, 14U, {1, 2});
  test::FillUploadField(upload.add_field(), 2799270304U, 36U, {2, 2});

  EXPECT_THAT(EncodeUploadRequest(*form_structure, available_field_types, false,
                                  std::string(), true),
              ElementsSerializeSameAs(upload));
}

TEST_F(AutofillCrowdsourcingEncoding, EncodeUploadRequest) {
  std::unique_ptr<FormStructure> form_structure;
  std::vector<FieldTypeSet> possible_field_types;
  std::vector<FieldTypeValidityStatesMap> possible_field_types_validities;
  FormData form;
  form.url = GURL("http://www.foo.com/");
  form.is_form_tag = true;

  FormFieldData field;
  field.form_control_type = FormControlType::kInputText;

  field.label = u"First Name";
  field.name = u"firstname";
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_FIRST});
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Last Name";
  field.name = u"lastname";
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_LAST});
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Email";
  field.name = u"email";
  field.form_control_type = FormControlType::kInputEmail;
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {EMAIL_ADDRESS});
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Phone";
  field.name = u"phone";
  field.form_control_type = FormControlType::kInputNumber;
  test::InitializePossibleTypesAndValidities(possible_field_types,
                                             possible_field_types_validities,
                                             {PHONE_HOME_WHOLE_NUMBER});
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Country";
  field.name = u"country";
  field.form_control_type = FormControlType::kSelectOne;
  test::InitializePossibleTypesAndValidities(possible_field_types,
                                             possible_field_types_validities,
                                             {ADDRESS_HOME_COUNTRY});
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  // Add checkable field.
  FormFieldData checkable_field;
  checkable_field.check_status =
      FormFieldData::CheckStatus::kCheckableButUnchecked;
  checkable_field.label = u"Checkable1";
  checkable_field.name = u"Checkable1";
  test::InitializePossibleTypesAndValidities(possible_field_types,
                                             possible_field_types_validities,
                                             {ADDRESS_HOME_COUNTRY});
  checkable_field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(checkable_field);

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->set_submission_event(
      SubmissionIndicatorEvent::HTML_FORM_SUBMISSION);
  for (auto& fs_field : *form_structure) {
    fs_field->host_form_signature = form_structure->form_signature();
  }

  ASSERT_EQ(form_structure->field_count(), possible_field_types.size());
  ASSERT_EQ(form_structure->field_count(),
            possible_field_types_validities.size());
  for (size_t i = 0; i < form_structure->field_count(); ++i) {
    form_structure->field(i)->set_possible_types(possible_field_types[i]);
    form_structure->field(i)->set_possible_types_validities(
        possible_field_types_validities[i]);
  }

  FieldTypeSet available_field_types;
  available_field_types.insert(NAME_FIRST);
  available_field_types.insert(NAME_LAST);
  available_field_types.insert(ADDRESS_HOME_LINE1);
  available_field_types.insert(ADDRESS_HOME_LINE2);
  available_field_types.insert(ADDRESS_HOME_COUNTRY);
  available_field_types.insert(EMAIL_ADDRESS);
  available_field_types.insert(PHONE_HOME_WHOLE_NUMBER);

  // Prepare the expected proto string.
  AutofillUploadContents upload;
  upload.set_submission(true);
  upload.set_submission_event(AutofillUploadContents::HTML_FORM_SUBMISSION);
  upload.set_client_version(
      std::string(GetProductNameAndVersionForUserAgent()));
  upload.set_form_signature(form_structure->form_signature().value());
  upload.set_autofill_used(false);
  upload.set_data_present("1442000308");
  upload.set_has_form_tag(true);

  test::FillUploadField(upload.add_field(), 3763331450U, 3U);
  test::FillUploadField(upload.add_field(), 3494530716U, 5U);
  test::FillUploadField(upload.add_field(), 1029417091U, 9U);
  test::FillUploadField(upload.add_field(), 466116101U, 14U);
  test::FillUploadField(upload.add_field(), 2799270304U, 36U);

  EXPECT_THAT(EncodeUploadRequest(*form_structure, available_field_types, false,
                                  std::string(), true),
              ElementsSerializeSameAs(upload));

  upload.set_autofill_used(true);
  EXPECT_THAT(EncodeUploadRequest(*form_structure, available_field_types, true,
                                  std::string(), true),
              ElementsSerializeSameAs(upload));

  // Add 2 address fields - this should be still a valid form.
  for (size_t i = 0; i < 2; ++i) {
    field.label = u"Address";
    field.name = u"address";
    field.form_control_type = FormControlType::kInputText;
    field.unique_renderer_id = test::MakeFieldRendererId();
    form.fields.push_back(field);
    test::InitializePossibleTypesAndValidities(
        possible_field_types, possible_field_types_validities,
        {ADDRESS_HOME_LINE1, ADDRESS_HOME_LINE2});
  }

  form_structure = std::make_unique<FormStructure>(form);
  form_structure->set_submission_event(
      SubmissionIndicatorEvent::HTML_FORM_SUBMISSION);
  ASSERT_EQ(form_structure->field_count(), possible_field_types.size());
  ASSERT_EQ(form_structure->field_count(),
            possible_field_types_validities.size());
  for (size_t i = 0; i < form_structure->field_count(); ++i) {
    form_structure->field(i)->host_form_signature =
        form_structure->form_signature();
    form_structure->field(i)->set_possible_types(possible_field_types[i]);
    form_structure->field(i)->set_possible_types_validities(
        possible_field_types_validities[i]);
  }

  // Adjust the expected proto string.
  upload.set_form_signature(form_structure->form_signature().value());
  upload.set_autofill_used(false);
  upload.set_submission_event(
      AutofillUploadContents_SubmissionIndicatorEvent_HTML_FORM_SUBMISSION);

  // Create an additional 2 fields (total of 7).
  for (int i = 0; i < 2; ++i) {
    test::FillUploadField(upload.add_field(), 509334676U, 30U);
  }
  // Put the appropriate autofill type on the different address fields.
  test::FillUploadField(upload.mutable_field(5), 509334676U, 31U);
  test::FillUploadField(upload.mutable_field(6), 509334676U, 31U);

  EXPECT_THAT(EncodeUploadRequest(*form_structure, available_field_types, false,
                                  std::string(), true),
              ElementsSerializeSameAs(upload));

  // Add 300 address fields - now the form is invalid, as it has too many
  // fields.
  for (size_t i = 0; i < 300; ++i) {
    field.label = u"Address";
    field.name = u"address";
    field.form_control_type = FormControlType::kInputText;
    field.unique_renderer_id = test::MakeFieldRendererId();
    form.fields.push_back(field);
    test::InitializePossibleTypesAndValidities(
        possible_field_types, possible_field_types_validities,
        {ADDRESS_HOME_LINE1, ADDRESS_HOME_LINE2});
  }
  form_structure = std::make_unique<FormStructure>(form);
  ASSERT_EQ(form_structure->field_count(), possible_field_types.size());
  ASSERT_EQ(form_structure->field_count(),
            possible_field_types_validities.size());
  for (size_t i = 0; i < form_structure->field_count(); ++i) {
    form_structure->field(i)->set_possible_types(possible_field_types[i]);
    form_structure->field(i)->set_possible_types_validities(
        possible_field_types_validities[i]);
  }

  EXPECT_TRUE(EncodeUploadRequest(*form_structure, available_field_types, false,
                                  std::string(), true)
                  .empty());
}

TEST_F(AutofillCrowdsourcingEncoding,
       EncodeUploadRequestWithAdditionalPasswordFormSignature) {
  std::unique_ptr<FormStructure> form_structure;
  std::vector<FieldTypeSet> possible_field_types;
  std::vector<FieldTypeValidityStatesMap> possible_field_types_validities;
  FormData form;
  form.url = GURL("http://www.foo.com/");
  form.is_form_tag = true;
  form.fields = {
      CreateTestFormField("First Name", "firstname", "",
                          FormControlType::kInputText, "given-name"),
      CreateTestFormField("Last Name", "lastname", "",
                          FormControlType::kInputText, "family-name"),
      CreateTestFormField("Email", "email", "", FormControlType::kInputEmail,
                          "email"),
      CreateTestFormField("username", "username", "",
                          FormControlType::kInputText, "email"),
      CreateTestFormField("password", "password", "",
                          FormControlType::kInputPassword, "email")};
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_FIRST});
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_LAST});
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {EMAIL_ADDRESS});
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {USERNAME});
  test::InitializePossibleTypesAndValidities(possible_field_types,
                                             possible_field_types_validities,
                                             {ACCOUNT_CREATION_PASSWORD});

  form_structure = std::make_unique<FormStructure>(form);
  for (auto& fs_field : *form_structure) {
    fs_field->host_form_signature = form_structure->form_signature();
  }

  ASSERT_EQ(form_structure->field_count(), possible_field_types.size());
  ASSERT_EQ(form_structure->field_count(),
            possible_field_types_validities.size());

  for (size_t i = 0; i < form_structure->field_count(); ++i) {
    form_structure->field(i)->set_possible_types(possible_field_types[i]);
    form_structure->field(i)->set_possible_types_validities(
        possible_field_types_validities[i]);

    if (form_structure->field(i)->name == u"password") {
      form_structure->field(i)->set_generation_type(
          AutofillUploadContents::Field::
              MANUALLY_TRIGGERED_GENERATION_ON_SIGN_UP_FORM);
      form_structure->field(i)->set_generated_password_changed(true);
    }
    if (form_structure->field(i)->name == u"username") {
      form_structure->field(i)->set_vote_type(
          AutofillUploadContents::Field::CREDENTIALS_REUSED);
    }
  }

  FieldTypeSet available_field_types;
  available_field_types.insert(NAME_FIRST);
  available_field_types.insert(NAME_LAST);
  available_field_types.insert(EMAIL_ADDRESS);
  available_field_types.insert(USERNAME);
  available_field_types.insert(ACCOUNT_CREATION_PASSWORD);

  // Prepare the expected proto string.
  AutofillUploadContents upload;
  upload.set_submission(true);
  upload.set_client_version(
      std::string(GetProductNameAndVersionForUserAgent()));
  upload.set_form_signature(form_structure->form_signature().value());
  upload.set_autofill_used(true);
  upload.set_data_present("1440000000000000000802");
  upload.set_login_form_signature(42);
  upload.set_submission_event(
      AutofillUploadContents_SubmissionIndicatorEvent_NONE);
  upload.set_has_form_tag(true);

  AutofillUploadContents::Field* upload_firstname_field = upload.add_field();
  test::FillUploadField(upload_firstname_field,
                        *form_structure->field(0)->GetFieldSignature(), 3U);

  AutofillUploadContents::Field* upload_lastname_field = upload.add_field();
  test::FillUploadField(upload_lastname_field,
                        *form_structure->field(1)->GetFieldSignature(), 5U);

  AutofillUploadContents::Field* upload_email_field = upload.add_field();
  test::FillUploadField(upload_email_field,
                        *form_structure->field(2)->GetFieldSignature(), 9U);

  AutofillUploadContents::Field* upload_username_field = upload.add_field();
  test::FillUploadField(upload_username_field,
                        *form_structure->field(3)->GetFieldSignature(), 86U);
  upload_username_field->set_vote_type(
      AutofillUploadContents::Field::CREDENTIALS_REUSED);

  AutofillUploadContents::Field* upload_password_field = upload.add_field();
  test::FillUploadField(upload_password_field,
                        *form_structure->field(4)->GetFieldSignature(), 76U);
  upload_password_field->set_generation_type(
      AutofillUploadContents::Field::
          MANUALLY_TRIGGERED_GENERATION_ON_SIGN_UP_FORM);
  upload_password_field->set_generated_password_changed(true);

  EXPECT_THAT(EncodeUploadRequest(*form_structure, available_field_types, true,
                                  "42", true),
              ElementsSerializeSameAs(upload));
}

TEST_F(AutofillCrowdsourcingEncoding, EncodeUploadRequestWithPropertiesMask) {
  std::unique_ptr<FormStructure> form_structure;
  std::vector<FieldTypeSet> possible_field_types;
  std::vector<FieldTypeValidityStatesMap> possible_field_types_validities;
  FormData form;
  form.url = GURL("http://www.foo.com/");
  form.is_form_tag = true;

  form.fields.push_back(CreateTestFormField("First Name", "firstname", "",
                                            FormControlType::kInputText,
                                            "given-name"));
  form.fields.back().name_attribute = form.fields.back().name;
  form.fields.back().id_attribute = u"first_name";
  form.fields.back().css_classes = u"class1 class2";
  form.fields.back().properties_mask = FieldPropertiesFlags::kHadFocus;
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_FIRST});

  form.fields.push_back(CreateTestFormField(
      "Last Name", "lastname", "", FormControlType::kInputText, "family-name"));
  form.fields.back().name_attribute = form.fields.back().name;
  form.fields.back().id_attribute = u"last_name";
  form.fields.back().css_classes = u"class1 class2";
  form.fields.back().properties_mask =
      FieldPropertiesFlags::kHadFocus | FieldPropertiesFlags::kUserTyped;
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_LAST});

  form.fields.push_back(CreateTestFormField(
      "Email", "email", "", FormControlType::kInputEmail, "email"));
  form.fields.back().name_attribute = form.fields.back().name;
  form.fields.back().id_attribute = u"e-mail";
  form.fields.back().css_classes = u"class1 class2";
  form.fields.back().properties_mask =
      FieldPropertiesFlags::kHadFocus | FieldPropertiesFlags::kUserTyped;
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {EMAIL_ADDRESS});

  form_structure = std::make_unique<FormStructure>(form);
  for (auto& fs_field : *form_structure) {
    fs_field->host_form_signature = form_structure->form_signature();
  }

  ASSERT_EQ(form_structure->field_count(), possible_field_types.size());
  ASSERT_EQ(form_structure->field_count(),
            possible_field_types_validities.size());

  for (size_t i = 0; i < form_structure->field_count(); ++i) {
    form_structure->field(i)->set_possible_types(possible_field_types[i]);
    form_structure->field(i)->set_possible_types_validities(
        possible_field_types_validities[i]);
  }

  FieldTypeSet available_field_types;
  available_field_types.insert(NAME_FIRST);
  available_field_types.insert(NAME_LAST);
  available_field_types.insert(EMAIL_ADDRESS);

  // Prepare the expected proto string.
  AutofillUploadContents upload;
  upload.set_submission(true);
  upload.set_client_version(
      std::string(GetProductNameAndVersionForUserAgent()));
  upload.set_form_signature(form_structure->form_signature().value());
  upload.set_autofill_used(true);
  upload.set_data_present("1440");
  upload.set_submission_event(
      AutofillUploadContents_SubmissionIndicatorEvent_NONE);
  upload.set_has_form_tag(true);

  test::FillUploadField(upload.add_field(), 3763331450U, 3U);
  upload.mutable_field(0)->set_properties_mask(FieldPropertiesFlags::kHadFocus);
  test::FillUploadField(upload.add_field(), 3494530716U, 5U);
  upload.mutable_field(1)->set_properties_mask(
      FieldPropertiesFlags::kHadFocus | FieldPropertiesFlags::kUserTyped);
  test::FillUploadField(upload.add_field(), 1029417091U, 9U);
  upload.mutable_field(2)->set_properties_mask(
      FieldPropertiesFlags::kHadFocus | FieldPropertiesFlags::kUserTyped);

  EXPECT_THAT(EncodeUploadRequest(*form_structure, available_field_types, true,
                                  std::string(), true),
              ElementsSerializeSameAs(upload));
}

TEST_F(AutofillCrowdsourcingEncoding,
       EncodeUploadRequest_ObservedSubmissionFalse) {
  std::unique_ptr<FormStructure> form_structure;
  std::vector<FieldTypeSet> possible_field_types;
  std::vector<FieldTypeValidityStatesMap> possible_field_types_validities;
  FormData form;
  form.url = GURL("http://www.foo.com/");
  form.is_form_tag = true;

  FormFieldData field;
  field.form_control_type = FormControlType::kInputText;

  field.label = u"First Name";
  field.name = u"firstname";
  field.name_attribute = field.name;
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_FIRST});

  field.label = u"Last Name";
  field.name = u"lastname";
  field.name_attribute = field.name;
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_LAST});

  field.label = u"Email";
  field.name = u"email";
  field.name_attribute = field.name;
  field.form_control_type = FormControlType::kInputEmail;
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {EMAIL_ADDRESS});

  form_structure = std::make_unique<FormStructure>(form);
  for (auto& fs_field : *form_structure) {
    fs_field->host_form_signature = form_structure->form_signature();
  }

  ASSERT_EQ(form_structure->field_count(), possible_field_types.size());
  ASSERT_EQ(form_structure->field_count(),
            possible_field_types_validities.size());

  for (size_t i = 0; i < form_structure->field_count(); ++i) {
    form_structure->field(i)->set_possible_types(possible_field_types[i]);
    form_structure->field(i)->set_possible_types_validities(
        possible_field_types_validities[i]);
  }

  FieldTypeSet available_field_types;
  available_field_types.insert(NAME_FIRST);
  available_field_types.insert(NAME_LAST);
  available_field_types.insert(EMAIL_ADDRESS);

  // Prepare the expected proto string.
  AutofillUploadContents upload;
  upload.set_submission(false);
  upload.set_client_version(
      std::string(GetProductNameAndVersionForUserAgent()));
  upload.set_form_signature(form_structure->form_signature().value());
  upload.set_autofill_used(true);
  upload.set_data_present("1440");
  upload.set_submission_event(
      AutofillUploadContents_SubmissionIndicatorEvent_NONE);
  upload.set_has_form_tag(true);

  test::FillUploadField(upload.add_field(), 3763331450U, 3U);
  test::FillUploadField(upload.add_field(), 3494530716U, 5U);
  test::FillUploadField(upload.add_field(), 1029417091U, 9U);

  EXPECT_THAT(EncodeUploadRequest(*form_structure, available_field_types, true,
                                  std::string(),
                                  /* observed_submission= */ false),
              ElementsSerializeSameAs(upload));
}

TEST_F(AutofillCrowdsourcingEncoding, EncodeUploadRequest_WithLabels) {
  std::unique_ptr<FormStructure> form_structure;
  std::vector<FieldTypeSet> possible_field_types;
  std::vector<FieldTypeValidityStatesMap> possible_field_types_validities;
  FormData form;
  form.url = GURL("http://www.foo.com/");
  form.is_form_tag = true;

  FormFieldData field;
  field.form_control_type = FormControlType::kInputText;

  // No label for the first field.
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_FIRST});

  field.label = u"Last Name";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_LAST});

  field.label = u"Email";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {EMAIL_ADDRESS});

  form_structure = std::make_unique<FormStructure>(form);
  for (auto& fs_field : *form_structure) {
    fs_field->host_form_signature = form_structure->form_signature();
  }

  ASSERT_EQ(form_structure->field_count(), possible_field_types.size());
  ASSERT_EQ(form_structure->field_count(),
            possible_field_types_validities.size());

  for (size_t i = 0; i < form_structure->field_count(); ++i) {
    form_structure->field(i)->set_possible_types(possible_field_types[i]);
    form_structure->field(i)->set_possible_types_validities(
        possible_field_types_validities[i]);
  }

  FieldTypeSet available_field_types;
  available_field_types.insert(NAME_FIRST);
  available_field_types.insert(NAME_LAST);
  available_field_types.insert(EMAIL_ADDRESS);

  // Prepare the expected proto string.
  AutofillUploadContents upload;
  upload.set_submission(true);
  upload.set_client_version(
      std::string(GetProductNameAndVersionForUserAgent()));
  upload.set_form_signature(form_structure->form_signature().value());
  upload.set_autofill_used(true);
  upload.set_data_present("1440");
  upload.set_submission_event(
      AutofillUploadContents_SubmissionIndicatorEvent_NONE);
  upload.set_has_form_tag(true);

  test::FillUploadField(upload.add_field(), 1318412689U, 3U);
  test::FillUploadField(upload.add_field(), 1318412689U, 5U);
  test::FillUploadField(upload.add_field(), 1318412689U, 9U);

  EXPECT_THAT(EncodeUploadRequest(*form_structure, available_field_types, true,
                                  std::string(), true),
              ElementsSerializeSameAs(upload));
}

// Tests that when the form is the result of flattening multiple forms into one,
// EncodeUploadRequest() returns multiple uploads: one for the entire form and
// one for each of the original forms.
TEST_F(AutofillCrowdsourcingEncoding, EncodeUploadRequest_WithSubForms) {
  std::unique_ptr<FormStructure> form_structure;
  std::vector<FieldTypeSet> possible_field_types;
  std::vector<FieldTypeValidityStatesMap> possible_field_types_validities;
  FormData form;
  form.host_frame = test::MakeLocalFrameToken();
  form.url = GURL("http://www.foo.com/");
  form.is_form_tag = true;

  FormFieldData field;
  field.form_control_type = FormControlType::kInputText;

  field.label = u"Cardholder name";
  field.name = u"cc-name";
  test::InitializePossibleTypesAndValidities(possible_field_types,
                                             possible_field_types_validities,
                                             {CREDIT_CARD_NAME_FULL});
  field.host_frame = form.host_frame;
  field.unique_renderer_id = test::MakeFieldRendererId();
  field.host_form_signature = FormSignature(123);
  form.fields.push_back(field);

  field.label = u"Credit card number";
  field.name = u"cc-number";
  test::InitializePossibleTypesAndValidities(possible_field_types,
                                             possible_field_types_validities,
                                             {CREDIT_CARD_NUMBER});
  field.host_frame = test::MakeLocalFrameToken();
  field.unique_renderer_id = test::MakeFieldRendererId();
  field.host_form_signature = FormSignature(456);
  form.fields.push_back(field);

  field.label = u"Expiration date";
  field.name = u"cc-exp";
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities,
      {CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR});
  field.host_frame = form.host_frame;
  field.unique_renderer_id = test::MakeFieldRendererId();
  field.host_form_signature = FormSignature(123);
  form.fields.push_back(field);

  field.label = u"CVC";
  field.name = u"cc-cvc";
  test::InitializePossibleTypesAndValidities(possible_field_types,
                                             possible_field_types_validities,
                                             {CREDIT_CARD_VERIFICATION_CODE});
  field.host_frame = test::MakeLocalFrameToken();
  field.unique_renderer_id = test::MakeFieldRendererId();
  field.host_form_signature = FormSignature(456);
  form.fields.push_back(field);

  ASSERT_EQ(form.global_id(), form.fields[0].renderer_form_id());
  ASSERT_NE(form.global_id(), form.fields[1].renderer_form_id());
  ASSERT_EQ(form.global_id(), form.fields[2].renderer_form_id());
  ASSERT_NE(form.global_id(), form.fields[3].renderer_form_id());

  form_structure = std::make_unique<FormStructure>(form);

  ASSERT_EQ(form_structure->field_count(), possible_field_types.size());
  ASSERT_EQ(form_structure->field_count(),
            possible_field_types_validities.size());
  for (size_t i = 0; i < form_structure->field_count(); ++i) {
    form_structure->field(i)->set_possible_types(possible_field_types[i]);
    form_structure->field(i)->set_possible_types_validities(
        possible_field_types_validities[i]);
  }

  FieldTypeSet available_field_types;
  available_field_types.insert(CREDIT_CARD_NAME_FULL);
  available_field_types.insert(CREDIT_CARD_NUMBER);
  available_field_types.insert(CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR);
  available_field_types.insert(CREDIT_CARD_VERIFICATION_CODE);

  // Prepare the expected proto string.
  const AutofillUploadContents upload_main = [&] {
    AutofillUploadContents upload;
    upload.set_submission(true);
    upload.set_submission_event(
        AutofillUploadContents_SubmissionIndicatorEvent_NONE);
    upload.set_client_version(
        std::string(GetProductNameAndVersionForUserAgent()));
    upload.set_form_signature(form_structure->form_signature().value());
    upload.set_autofill_used(false);
    upload.set_data_present("0000000000001850");
    upload.set_has_form_tag(true);
    test::FillUploadField(upload.add_field(), 3340391946, 51);
    test::FillUploadField(upload.add_field(), 1415886167, 52);
    test::FillUploadField(upload.add_field(), 3155194603, 57);
    test::FillUploadField(upload.add_field(), 917221285, 59);
    return upload;
  }();

  const AutofillUploadContents upload_name_exp = [&] {
    AutofillUploadContents upload;
    upload.set_client_version(
        std::string(GetProductNameAndVersionForUserAgent()));
    upload.set_form_signature(form.fields[0].host_form_signature.value());
    upload.set_autofill_used(false);
    upload.set_data_present("0000000000001850");
    test::FillUploadField(upload.add_field(), 3340391946, 51);
    test::FillUploadField(upload.add_field(), 3155194603, 57);
    return upload;
  }();

  const AutofillUploadContents upload_number = [&] {
    AutofillUploadContents upload;
    upload.set_client_version(
        std::string(GetProductNameAndVersionForUserAgent()));
    upload.set_form_signature(form.fields[1].host_form_signature.value());
    upload.set_autofill_used(false);
    upload.set_data_present("0000000000001850");
    test::FillUploadField(upload.add_field(), 1415886167, 52);
    return upload;
  }();

  const AutofillUploadContents upload_cvc = [&] {
    AutofillUploadContents upload;
    upload.set_client_version(
        std::string(GetProductNameAndVersionForUserAgent()));
    upload.set_form_signature(form.fields[3].host_form_signature.value());
    upload.set_autofill_used(false);
    upload.set_data_present("0000000000001850");
    test::FillUploadField(upload.add_field(), 917221285, 59);
    return upload;
  }();

  EXPECT_THAT(EncodeUploadRequest(*form_structure, available_field_types, false,
                                  std::string(), true),
              UnorderedElementsSerializeSameAs(upload_main, upload_name_exp,
                                               upload_number, upload_cvc));
}

// Check that we compute the "datapresent" string correctly for the given
// |available_types|.
TEST_F(AutofillCrowdsourcingEncoding, CheckDataPresence) {
  FormData form;
  form.url = GURL("http://www.foo.com/");
  form.is_form_tag = true;

  FormFieldData field;
  field.form_control_type = FormControlType::kInputText;

  field.label = u"First Name";
  field.name = u"first";
  field.name_attribute = field.name;
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Last Name";
  field.name = u"last";
  field.name_attribute = field.name;
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.label = u"Email";
  field.name = u"email";
  field.name_attribute = field.name;
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.set_submission_source(SubmissionSource::FORM_SUBMISSION);
  for (auto& fs_field : form_structure) {
    fs_field->host_form_signature = form_structure.form_signature();
  }

  std::vector<FieldTypeSet> possible_field_types;
  std::vector<FieldTypeValidityStatesMap> possible_field_types_validities;

  for (size_t i = 0; i < form_structure.field_count(); ++i) {
    test::InitializePossibleTypesAndValidities(
        possible_field_types, possible_field_types_validities, {UNKNOWN_TYPE});
    form_structure.field(i)->set_possible_types(possible_field_types[i]);
    form_structure.field(i)->set_possible_types_validities(
        possible_field_types_validities[i]);
  }

  // No available types.
  // datapresent should be "" == trimmed(0x0000000000000000) ==
  //     0b0000000000000000000000000000000000000000000000000000000000000000
  FieldTypeSet available_field_types;

  // Prepare the expected proto string.
  AutofillUploadContents upload;
  upload.set_submission(true);
  upload.set_client_version(
      std::string(GetProductNameAndVersionForUserAgent()));
  upload.set_form_signature(form_structure.form_signature().value());
  upload.set_autofill_used(false);
  upload.set_data_present("");
  upload.set_submission_event(
      AutofillUploadContents_SubmissionIndicatorEvent_HTML_FORM_SUBMISSION);
  upload.set_has_form_tag(true);

  test::FillUploadField(upload.add_field(), 1089846351U, 1U);
  test::FillUploadField(upload.add_field(), 2404144663U, 1U);
  test::FillUploadField(upload.add_field(), 420638584U, 1U);

  EXPECT_THAT(EncodeUploadRequest(form_structure, available_field_types, false,
                                  std::string(), true),
              ElementsSerializeSameAs(upload));

  // Only a few types available.
  // datapresent should be "1540000240" == trimmed(0x1540000240000000) ==
  //     0b0001010101000000000000000000001001000000000000000000000000000000
  // The set bits are:
  //  3 == NAME_FIRST
  //  5 == NAME_LAST
  //  7 == NAME_FULL
  //  9 == EMAIL_ADDRESS
  // 30 == ADDRESS_HOME_LINE1
  // 33 == ADDRESS_HOME_CITY
  available_field_types.clear();
  available_field_types.insert(NAME_FIRST);
  available_field_types.insert(NAME_LAST);
  available_field_types.insert(NAME_FULL);
  available_field_types.insert(EMAIL_ADDRESS);
  available_field_types.insert(ADDRESS_HOME_LINE1);
  available_field_types.insert(ADDRESS_HOME_CITY);

  // Adjust the expected proto string.
  upload.set_data_present("1540000240");
  EXPECT_THAT(EncodeUploadRequest(form_structure, available_field_types, false,
                                  std::string(), true),
              ElementsSerializeSameAs(upload));

  // All supported non-credit card types available.
  // datapresent should be "1f7e000378000008" == trimmed(0x1f7e000378000008) ==
  //     0b0001111101111110000000000000001101111000000000000000000000001000
  // The set bits are:
  //  3 == NAME_FIRST
  //  4 == NAME_MIDDLE
  //  5 == NAME_LAST
  //  6 == NAME_MIDDLE_INITIAL
  //  7 == NAME_FULL
  //  9 == EMAIL_ADDRESS
  // 10 == PHONE_HOME_NUMBER,
  // 11 == PHONE_HOME_CITY_CODE,
  // 12 == PHONE_HOME_COUNTRY_CODE,
  // 13 == PHONE_HOME_CITY_AND_NUMBER,
  // 14 == PHONE_HOME_WHOLE_NUMBER,
  // 30 == ADDRESS_HOME_LINE1
  // 31 == ADDRESS_HOME_LINE2
  // 33 == ADDRESS_HOME_CITY
  // 34 == ADDRESS_HOME_STATE
  // 35 == ADDRESS_HOME_ZIP
  // 36 == ADDRESS_HOME_COUNTRY
  // 60 == COMPANY_NAME
  available_field_types.clear();
  available_field_types.insert(NAME_FIRST);
  available_field_types.insert(NAME_MIDDLE);
  available_field_types.insert(NAME_LAST);
  available_field_types.insert(NAME_MIDDLE_INITIAL);
  available_field_types.insert(NAME_FULL);
  available_field_types.insert(EMAIL_ADDRESS);
  available_field_types.insert(PHONE_HOME_NUMBER);
  available_field_types.insert(PHONE_HOME_CITY_CODE);
  available_field_types.insert(PHONE_HOME_COUNTRY_CODE);
  available_field_types.insert(PHONE_HOME_CITY_AND_NUMBER);
  available_field_types.insert(PHONE_HOME_WHOLE_NUMBER);
  available_field_types.insert(ADDRESS_HOME_LINE1);
  available_field_types.insert(ADDRESS_HOME_LINE2);
  available_field_types.insert(ADDRESS_HOME_CITY);
  available_field_types.insert(ADDRESS_HOME_STATE);
  available_field_types.insert(ADDRESS_HOME_ZIP);
  available_field_types.insert(ADDRESS_HOME_COUNTRY);
  available_field_types.insert(COMPANY_NAME);

  // Adjust the expected proto string.
  upload.set_data_present("1f7e000378000008");
  EXPECT_THAT(EncodeUploadRequest(form_structure, available_field_types, false,
                                  std::string(), true),
              ElementsSerializeSameAs(upload));

  // All supported credit card types available.
  // datapresent should be "0000000000001fc0" == trimmed(0x0000000000001fc0) ==
  //     0b0000000000000000000000000000000000000000000000000001111111000000
  // The set bits are:
  // 51 == CREDIT_CARD_NAME_FULL
  // 52 == CREDIT_CARD_NUMBER
  // 53 == CREDIT_CARD_EXP_MONTH
  // 54 == CREDIT_CARD_EXP_2_DIGIT_YEAR
  // 55 == CREDIT_CARD_EXP_4_DIGIT_YEAR
  // 56 == CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR
  // 57 == CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR
  available_field_types.clear();
  available_field_types.insert(CREDIT_CARD_NAME_FULL);
  available_field_types.insert(CREDIT_CARD_NUMBER);
  available_field_types.insert(CREDIT_CARD_EXP_MONTH);
  available_field_types.insert(CREDIT_CARD_EXP_2_DIGIT_YEAR);
  available_field_types.insert(CREDIT_CARD_EXP_4_DIGIT_YEAR);
  available_field_types.insert(CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR);
  available_field_types.insert(CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR);

  // Adjust the expected proto string.
  upload.set_data_present("0000000000001fc0");
  EXPECT_THAT(EncodeUploadRequest(form_structure, available_field_types, false,
                                  std::string(), true),
              ElementsSerializeSameAs(upload));

  // All supported types available.
  // datapresent should be "1f7e000378001fc8" == trimmed(0x1f7e000378001fc8) ==
  //     0b0001111101111110000000000000001101111000000000000001111111001000
  // The set bits are:
  //  3 == NAME_FIRST
  //  4 == NAME_MIDDLE
  //  5 == NAME_LAST
  //  6 == NAME_MIDDLE_INITIAL
  //  7 == NAME_FULL
  //  9 == EMAIL_ADDRESS
  // 10 == PHONE_HOME_NUMBER,
  // 11 == PHONE_HOME_CITY_CODE,
  // 12 == PHONE_HOME_COUNTRY_CODE,
  // 13 == PHONE_HOME_CITY_AND_NUMBER,
  // 14 == PHONE_HOME_WHOLE_NUMBER,
  // 30 == ADDRESS_HOME_LINE1
  // 31 == ADDRESS_HOME_LINE2
  // 33 == ADDRESS_HOME_CITY
  // 34 == ADDRESS_HOME_STATE
  // 35 == ADDRESS_HOME_ZIP
  // 36 == ADDRESS_HOME_COUNTRY
  // 51 == CREDIT_CARD_NAME_FULL
  // 52 == CREDIT_CARD_NUMBER
  // 53 == CREDIT_CARD_EXP_MONTH
  // 54 == CREDIT_CARD_EXP_2_DIGIT_YEAR
  // 55 == CREDIT_CARD_EXP_4_DIGIT_YEAR
  // 56 == CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR
  // 57 == CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR
  // 60 == COMPANY_NAME
  available_field_types.clear();
  available_field_types.insert(NAME_FIRST);
  available_field_types.insert(NAME_MIDDLE);
  available_field_types.insert(NAME_LAST);
  available_field_types.insert(NAME_MIDDLE_INITIAL);
  available_field_types.insert(NAME_FULL);
  available_field_types.insert(EMAIL_ADDRESS);
  available_field_types.insert(PHONE_HOME_NUMBER);
  available_field_types.insert(PHONE_HOME_CITY_CODE);
  available_field_types.insert(PHONE_HOME_COUNTRY_CODE);
  available_field_types.insert(PHONE_HOME_CITY_AND_NUMBER);
  available_field_types.insert(PHONE_HOME_WHOLE_NUMBER);
  available_field_types.insert(ADDRESS_HOME_LINE1);
  available_field_types.insert(ADDRESS_HOME_LINE2);
  available_field_types.insert(ADDRESS_HOME_CITY);
  available_field_types.insert(ADDRESS_HOME_STATE);
  available_field_types.insert(ADDRESS_HOME_ZIP);
  available_field_types.insert(ADDRESS_HOME_COUNTRY);
  available_field_types.insert(CREDIT_CARD_NAME_FULL);
  available_field_types.insert(CREDIT_CARD_NUMBER);
  available_field_types.insert(CREDIT_CARD_EXP_MONTH);
  available_field_types.insert(CREDIT_CARD_EXP_2_DIGIT_YEAR);
  available_field_types.insert(CREDIT_CARD_EXP_4_DIGIT_YEAR);
  available_field_types.insert(CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR);
  available_field_types.insert(CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR);
  available_field_types.insert(COMPANY_NAME);

  // Adjust the expected proto string.
  upload.set_data_present("1f7e000378001fc8");
  EXPECT_THAT(EncodeUploadRequest(form_structure, available_field_types, false,
                                  std::string(), true),
              ElementsSerializeSameAs(upload));
}

TEST_F(AutofillCrowdsourcingEncoding, CheckMultipleTypes) {
  // Throughout this test, datapresent should be
  // 0x1440000360000008 ==
  //     0b0001010001000000000000000000001101100000000000000000000000001000
  // The set bits are:
  //  3 == NAME_FIRST
  //  5 == NAME_LAST
  //  9 == EMAIL_ADDRESS
  // 30 == ADDRESS_HOME_LINE1
  // 31 == ADDRESS_HOME_LINE2
  // 33 == ADDRESS_HOME_CITY
  // 34 == ADDRESS_HOME_STATE
  // 60 == COMPANY_NAME
  FieldTypeSet available_field_types;
  available_field_types.insert(NAME_FIRST);
  available_field_types.insert(NAME_LAST);
  available_field_types.insert(EMAIL_ADDRESS);
  available_field_types.insert(ADDRESS_HOME_LINE1);
  available_field_types.insert(ADDRESS_HOME_LINE2);
  available_field_types.insert(ADDRESS_HOME_CITY);
  available_field_types.insert(ADDRESS_HOME_STATE);
  available_field_types.insert(COMPANY_NAME);

  // Check that multiple types for the field are processed correctly.
  std::vector<FieldTypeSet> possible_field_types;
  std::vector<FieldTypeValidityStatesMap> possible_field_types_validities;
  FormData form;
  form.url = GURL("http://www.foo.com/");
  form.is_form_tag = false;

  FormFieldData field;
  field.form_control_type = FormControlType::kInputText;

  field.label = u"email";
  field.name = u"email";
  field.name_attribute = field.name;
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {EMAIL_ADDRESS});

  field.label = u"First Name";
  field.name = u"first";
  field.name_attribute = field.name;
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_FIRST});

  field.label = u"Last Name";
  field.name = u"last";
  field.name_attribute = field.name;
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(
      possible_field_types, possible_field_types_validities, {NAME_LAST});

  field.label = u"Address";
  field.name = u"address";
  field.name_attribute = field.name;
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);
  test::InitializePossibleTypesAndValidities(possible_field_types,
                                             possible_field_types_validities,
                                             {ADDRESS_HOME_LINE1});

  auto form_structure = std::make_unique<FormStructure>(form);
  form_structure->set_submission_source(SubmissionSource::XHR_SUCCEEDED);
  for (size_t i = 0; i < form_structure->field_count(); ++i) {
    form_structure->field(i)->host_form_signature =
        form_structure->form_signature();
    form_structure->field(i)->set_possible_types(possible_field_types[i]);
    form_structure->field(i)->set_possible_types_validities(
        possible_field_types_validities[i]);
  }

  // Prepare the expected proto string.
  AutofillUploadContents upload;
  upload.set_submission(true);
  upload.set_client_version(
      std::string(GetProductNameAndVersionForUserAgent()));
  upload.set_form_signature(form_structure->form_signature().value());
  upload.set_autofill_used(false);
  upload.set_data_present("1440000360000008");
  upload.set_has_form_tag(false);
  upload.set_submission_event(
      AutofillUploadContents_SubmissionIndicatorEvent_XHR_SUCCEEDED);

  test::FillUploadField(upload.add_field(), 420638584U, 9U);
  test::FillUploadField(upload.add_field(), 1089846351U, 3U);
  test::FillUploadField(upload.add_field(), 2404144663U, 5U);
  test::FillUploadField(upload.add_field(), 509334676U, 30U);

  EXPECT_THAT(EncodeUploadRequest(*form_structure, available_field_types, false,
                                  std::string(), true),
              ElementsSerializeSameAs(upload));

  // Match third field as both first and last.
  possible_field_types[2].insert(NAME_FIRST);
  form_structure->field(2)->set_possible_types(possible_field_types[2]);

  // Modify the expected upload.
  // Add the NAME_FIRST prediction to the third field.
  test::FillUploadField(upload.mutable_field(2), 2404144663U, 3U);

  upload.mutable_field(2)->mutable_autofill_type()->SwapElements(0, 1);
  upload.mutable_field(2)->mutable_autofill_type_validities()->SwapElements(0,
                                                                            1);

  EXPECT_THAT(EncodeUploadRequest(*form_structure, available_field_types, false,
                                  std::string(), true),
              ElementsSerializeSameAs(upload));

  // Match last field as both address home line 1 and 2.
  possible_field_types[3].insert(ADDRESS_HOME_LINE2);
  form_structure->field(form_structure->field_count() - 1)
      ->set_possible_types(
          possible_field_types[form_structure->field_count() - 1]);

  // Adjust the expected upload proto.
  test::FillUploadField(upload.mutable_field(3), 509334676U, 31U);

  EXPECT_THAT(EncodeUploadRequest(*form_structure, available_field_types, false,
                                  std::string(), true),
              ElementsSerializeSameAs(upload));

  // Replace the address line 2 prediction by company name.
  possible_field_types[3].clear();
  possible_field_types[3].insert(ADDRESS_HOME_LINE1);
  possible_field_types[3].insert(COMPANY_NAME);
  form_structure->field(form_structure->field_count() - 1)
      ->set_possible_types(
          possible_field_types[form_structure->field_count() - 1]);
  possible_field_types_validities[3].clear();
  form_structure->field(form_structure->field_count() - 1)
      ->set_possible_types_validities(
          possible_field_types_validities[form_structure->field_count() - 1]);

  // Adjust the expected upload proto.
  upload.mutable_field(3)->mutable_autofill_type_validities(1)->set_type(60);
  upload.mutable_field(3)->set_autofill_type(1, 60);

  EXPECT_THAT(EncodeUploadRequest(*form_structure, available_field_types, false,
                                  std::string(), true),
              ElementsSerializeSameAs(upload));
}

TEST_F(AutofillCrowdsourcingEncoding, EncodeUploadRequest_PasswordsRevealed) {
  FormData form;
  form.url = GURL("http://www.foo.com/");

  // Add 3 fields, to make the form uploadable.
  FormFieldData field;
  field.name = u"email";
  field.name_attribute = field.name;
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.name = u"first";
  field.name_attribute = field.name;
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  field.name = u"last";
  field.name_attribute = field.name;
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  FormStructure form_structure(form);
  for (auto& fs_field : form_structure) {
    fs_field->host_form_signature = form_structure.form_signature();
  }

  std::vector<AutofillUploadContents> uploads = EncodeUploadRequest(
      form_structure, {{}} /* available_field_types */,
      false /* form_was_autofilled */, std::string() /* login_form_signature */,
      true /* observed_submission */);
  ASSERT_EQ(1u, uploads.size());
}

TEST_F(AutofillCrowdsourcingEncoding, EncodeUploadRequest_IsFormTag) {
  for (bool is_form_tag : {false, true}) {
    SCOPED_TRACE(testing::Message() << "is_form_tag=" << is_form_tag);

    FormData form;
    form.url = GURL("http://www.foo.com/");
    FormFieldData field;
    field.name = u"email";
    field.unique_renderer_id = test::MakeFieldRendererId();
    form.fields.push_back(field);

    form.is_form_tag = is_form_tag;

    FormStructure form_structure(form);
    for (auto& fs_field : form_structure) {
      fs_field->host_form_signature = form_structure.form_signature();
    }
    std::vector<AutofillUploadContents> uploads =
        EncodeUploadRequest(form_structure, {{}} /* available_field_types */,
                            false /* form_was_autofilled */,
                            std::string() /* login_form_signature */,
                            true /* observed_submission */);
    ASSERT_EQ(1u, uploads.size());
    EXPECT_EQ(is_form_tag, uploads.front().has_form_tag());
  }
}

TEST_F(AutofillCrowdsourcingEncoding, EncodeUploadRequest_RichMetadata) {
  struct FieldMetadata {
    const char *id, *name, *label, *placeholder, *aria_label, *aria_description,
        *css_classes, *autocomplete;
  };

  static const FieldMetadata kFieldMetadata[] = {
      {"fname_id", "fname_name", "First Name:", "Please enter your first name",
       "Type your first name", "You can type your first name here", "blah",
       "given-name"},
      {"lname_id", "lname_name", "Last Name:", "Please enter your last name",
       "Type your lat name", "You can type your last name here", "blah",
       "family-name"},
      {"email_id", "email_name", "Email:", "Please enter your email address",
       "Type your email address", "You can type your email address here",
       "blah", "email"},
      {"id_only", "", "", "", "", "", "", ""},
      {"", "name_only", "", "", "", "", "", ""},
  };

  FormData form;
  form.id_attribute = u"form-id";
  form.url = GURL("http://www.foo.com/");
  form.button_titles = {std::make_pair(
      u"Submit", mojom::ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE)};
  form.full_url = GURL("http://www.foo.com/?foo=bar");
  for (const auto& f : kFieldMetadata) {
    FormFieldData field;
    field.id_attribute = ASCIIToUTF16(f.id);
    field.name_attribute = ASCIIToUTF16(f.name);
    field.name = field.name_attribute;
    field.label = ASCIIToUTF16(f.label);
    field.placeholder = ASCIIToUTF16(f.placeholder);
    field.aria_label = ASCIIToUTF16(f.aria_label);
    field.aria_description = ASCIIToUTF16(f.aria_description);
    field.css_classes = ASCIIToUTF16(f.css_classes);
    field.autocomplete_attribute = f.autocomplete;
    field.parsed_autocomplete = ParseAutocompleteAttribute(f.autocomplete);
    field.unique_renderer_id = test::MakeFieldRendererId();
    form.fields.push_back(field);
  }
  RandomizedEncoder encoder("seed for testing",
                            AutofillRandomizedValue_EncodingType_ALL_BITS,
                            /*anonymous_url_collection_is_enabled*/ true);

  FormStructure form_structure(form);
  form_structure.set_randomized_encoder(
      std::make_unique<RandomizedEncoder>(encoder));
  for (auto& field : form_structure) {
    field->host_form_signature = form_structure.form_signature();
  }

  std::vector<AutofillUploadContents> uploads = EncodeUploadRequest(
      form_structure, {{}} /* available_field_types */,
      false /* form_was_autofilled */, std::string() /* login_form_signature */,
      true /* observed_submission */);
  ASSERT_EQ(1u, uploads.size());
  AutofillUploadContents& upload = uploads.front();

  const auto form_signature = form_structure.form_signature();

  if (form.id_attribute.empty()) {
    EXPECT_FALSE(upload.randomized_form_metadata().has_id());
  } else {
    EXPECT_EQ(upload.randomized_form_metadata().id().encoded_bits(),
              encoder.EncodeForTesting(form_signature, FieldSignature(),
                                       RandomizedEncoder::FORM_ID,
                                       form_structure.id_attribute()));
  }

  if (form.name_attribute.empty()) {
    EXPECT_FALSE(upload.randomized_form_metadata().has_name());
  } else {
    EXPECT_EQ(upload.randomized_form_metadata().name().encoded_bits(),
              encoder.EncodeForTesting(form_signature, FieldSignature(),
                                       RandomizedEncoder::FORM_NAME,
                                       form_structure.name_attribute()));
  }

  auto full_url = form_structure.full_source_url().spec();
  EXPECT_EQ(upload.randomized_form_metadata().url().encoded_bits(),
            encoder.Encode(form_signature, FieldSignature(),
                           RandomizedEncoder::FORM_URL, full_url));
  ASSERT_EQ(static_cast<size_t>(upload.field_size()),
            std::size(kFieldMetadata));

  ASSERT_EQ(1, upload.randomized_form_metadata().button_title().size());
  EXPECT_EQ(upload.randomized_form_metadata()
                .button_title()[0]
                .title()
                .encoded_bits(),
            encoder.EncodeForTesting(form_signature, FieldSignature(),
                                     RandomizedEncoder::FORM_BUTTON_TITLES,
                                     form.button_titles[0].first));
  EXPECT_EQ(ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE,
            upload.randomized_form_metadata().button_title()[0].type());

  for (int i = 0; i < upload.field_size(); ++i) {
    const auto& metadata = upload.field(i).randomized_field_metadata();
    const auto& field = *form_structure.field(i);
    const auto field_signature = field.GetFieldSignature();
    if (field.id_attribute.empty()) {
      EXPECT_FALSE(metadata.has_id());
    } else {
      EXPECT_EQ(metadata.id().encoded_bits(),
                encoder.EncodeForTesting(form_signature, field_signature,
                                         RandomizedEncoder::FIELD_ID,
                                         field.id_attribute));
    }
    if (field.name.empty()) {
      EXPECT_FALSE(metadata.has_name());
    } else {
      EXPECT_EQ(metadata.name().encoded_bits(),
                encoder.EncodeForTesting(form_signature, field_signature,
                                         RandomizedEncoder::FIELD_NAME,
                                         field.name_attribute));
    }
    EXPECT_EQ(metadata.type().encoded_bits(),
              encoder.Encode(form_signature, field_signature,
                             RandomizedEncoder::FIELD_CONTROL_TYPE,
                             FormControlTypeToString(field.form_control_type)));
    if (field.label.empty()) {
      EXPECT_FALSE(metadata.has_label());
    } else {
      EXPECT_EQ(metadata.label().encoded_bits(),
                encoder.EncodeForTesting(form_signature, field_signature,
                                         RandomizedEncoder::FIELD_LABEL,
                                         field.label));
    }
    if (field.aria_label.empty()) {
      EXPECT_FALSE(metadata.has_aria_label());
    } else {
      EXPECT_EQ(metadata.aria_label().encoded_bits(),
                encoder.EncodeForTesting(form_signature, field_signature,
                                         RandomizedEncoder::FIELD_ARIA_LABEL,
                                         field.aria_label));
    }
    if (field.aria_description.empty()) {
      EXPECT_FALSE(metadata.has_aria_description());
    } else {
      EXPECT_EQ(
          metadata.aria_description().encoded_bits(),
          encoder.EncodeForTesting(form_signature, field_signature,
                                   RandomizedEncoder::FIELD_ARIA_DESCRIPTION,
                                   field.aria_description));
    }
    if (field.css_classes.empty()) {
      EXPECT_FALSE(metadata.has_css_class());
    } else {
      EXPECT_EQ(metadata.css_class().encoded_bits(),
                encoder.EncodeForTesting(form_signature, field_signature,
                                         RandomizedEncoder::FIELD_CSS_CLASS,
                                         field.css_classes));
    }
    if (field.placeholder.empty()) {
      EXPECT_FALSE(metadata.has_placeholder());
    } else {
      EXPECT_EQ(metadata.placeholder().encoded_bits(),
                encoder.EncodeForTesting(form_signature, field_signature,
                                         RandomizedEncoder::FIELD_PLACEHOLDER,
                                         field.placeholder));
    }
    if (field.autocomplete_attribute.empty()) {
      EXPECT_FALSE(metadata.has_autocomplete());
    } else {
      EXPECT_EQ(metadata.autocomplete().encoded_bits(),
                encoder.EncodeForTesting(
                    form_signature, field_signature,
                    RandomizedEncoder::FIELD_AUTOCOMPLETE,
                    base::UTF8ToUTF16(field.autocomplete_attribute)));
    }
  }
}

TEST_F(AutofillCrowdsourcingEncoding, Metadata_OnlySendFullUrlWithUserConsent) {
  for (bool has_consent : {true, false}) {
    SCOPED_TRACE(testing::Message() << " has_consent=" << has_consent);
    FormData form;
    form.id_attribute = u"form-id";
    form.url = GURL("http://www.foo.com/");
    form.full_url = GURL("http://www.foo.com/?foo=bar");

    // One form field needed to be valid form.
    FormFieldData field;
    field.form_control_type = FormControlType::kInputText;
    field.label = u"email";
    field.name = u"email";
    field.unique_renderer_id = test::MakeFieldRendererId();
    form.fields.push_back(field);

    TestingPrefServiceSimple prefs;
    prefs.registry()->RegisterBooleanPref(
        RandomizedEncoder::kUrlKeyedAnonymizedDataCollectionEnabled, false);
    prefs.SetBoolean(
        RandomizedEncoder::kUrlKeyedAnonymizedDataCollectionEnabled,
        has_consent);
    prefs.registry()->RegisterStringPref(prefs::kAutofillUploadEncodingSeed,
                                         "default_secret");
    prefs.SetString(prefs::kAutofillUploadEncodingSeed, "user_secret");

    FormStructure form_structure(form);
    form_structure.set_randomized_encoder(RandomizedEncoder::Create(&prefs));
    std::vector<AutofillUploadContents> uploads =
        EncodeUploadRequest(form_structure, {}, true, "", true);

    EXPECT_EQ(has_consent,
              uploads.front().randomized_form_metadata().has_url());
  }
}

TEST_F(AutofillCrowdsourcingEncoding,
       EncodeUploadRequest_WithSingleUsernameVoteType) {
  FormData form;
  form.url = GURL("http://www.foo.com/");
  FormFieldData field;
  field.name = u"text field";
  field.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.field(0)->set_single_username_vote_type(
      AutofillUploadContents::Field::STRONG);
  form_structure.field(0)->set_is_most_recent_single_username_candidate(
      IsMostRecentSingleUsernameCandidate::kMostRecentCandidate);
  for (auto& fs_field : form_structure) {
    fs_field->host_form_signature = form_structure.form_signature();
  }

  std::vector<AutofillUploadContents> uploads = EncodeUploadRequest(
      form_structure, {{}} /* available_field_types */,
      false /* form_was_autofilled */, std::string() /* login_form_signature */,
      true /* observed_submission */);
  ASSERT_EQ(1u, uploads.size());
  EXPECT_EQ(form_structure.field(0)->single_username_vote_type(),
            uploads.front().field(0).single_username_vote_type());
  EXPECT_TRUE(
      uploads.front().field(0).is_most_recent_single_username_candidate());
}

TEST_F(AutofillCrowdsourcingEncoding,
       EncodeUploadRequest_WithSingleUsernameData) {
  FormData form;
  form.url = GURL("http://www.foo.com/");
  FormFieldData field_data;
  field_data.name = u"text field";
  field_data.unique_renderer_id = test::MakeFieldRendererId();
  form.fields.push_back(field_data);

  FormStructure form_structure(form);
  for (auto& field : form_structure) {
    field->host_form_signature = form_structure.form_signature();
  }

  AutofillUploadContents::SingleUsernameData single_username_data;
  single_username_data.set_username_form_signature(12345);
  single_username_data.set_username_field_signature(678910);
  single_username_data.set_value_type(AutofillUploadContents::EMAIL);
  single_username_data.set_prompt_edit(AutofillUploadContents::EDITED_POSITIVE);
  form_structure.AddSingleUsernameData(single_username_data);

  std::vector<AutofillUploadContents> uploads = EncodeUploadRequest(
      form_structure, {{}} /* available_field_types */,
      false /* form_was_autofilled */, std::string() /* login_form_signature */,
      true /* observed_submission */);
  ASSERT_EQ(1u, uploads.size());
  ASSERT_EQ(1, uploads.front().single_username_data().size());
  const AutofillUploadContents::SingleUsernameData& uploaded_data =
      uploads.front().single_username_data()[0];
  EXPECT_EQ(single_username_data.username_form_signature(),
            uploaded_data.username_form_signature());
  EXPECT_EQ(single_username_data.username_field_signature(),
            uploaded_data.username_field_signature());
  EXPECT_EQ(single_username_data.value_type(), uploaded_data.value_type());
  EXPECT_EQ(single_username_data.prompt_edit(), uploaded_data.prompt_edit());
}

// Checks that CreateForPasswordManagerUpload builds FormStructure
// which is encodable (i.e. ready for uploading).
TEST_F(AutofillCrowdsourcingEncoding, CreateForPasswordManagerUpload) {
  std::unique_ptr<FormStructure> form =
      FormStructure::CreateForPasswordManagerUpload(
          FormSignature(1234),
          {FieldSignature(1), FieldSignature(10), FieldSignature(100)});
  for (auto& field : *form) {
    field->host_form_signature = form->form_signature();
  }
  EXPECT_EQ(FormSignature(1234u), form->form_signature());
  ASSERT_EQ(3u, form->field_count());
  ASSERT_EQ(FieldSignature(100u), form->field(2)->GetFieldSignature());
  std::vector<AutofillUploadContents> uploads = EncodeUploadRequest(
      *form, {} /* available_field_types */, false /* form_was_autofilled */,
      "" /*login_form_signature*/, true /*observed_submission*/);
  ASSERT_EQ(1u, uploads.size());
}

// Milestone number must be set to correct actual value, as autofill server
// relies on this. If this is planning to change, inform Autofill team. This
// must be set to avoid situations similar to dropping branch number in M101,
// which yielded cl/513794193 and cl/485660167.
TEST_F(AutofillCrowdsourcingEncoding, EncodeUploadRequest_MilestoneSet) {
  // To test |EncodeUploadRequest()|, a non-empty form is required.
  std::unique_ptr<FormStructure> form =
      FormStructure::CreateForPasswordManagerUpload(FormSignature(1234),
                                                    {FieldSignature(1)});
  for (auto& field : *form) {
    field->host_form_signature = form->form_signature();
  }
  std::vector<AutofillUploadContents> uploads = EncodeUploadRequest(
      *form, {} /* available_field_types */, false /* form_was_autofilled */,
      "" /*login_form_signature*/, true /*observed_submission*/);
  ASSERT_EQ(1u, uploads.size());
  static constexpr char kChromeVersionRegex[] =
      "\\w+/([0-9]+)\\.[0-9]+\\.[0-9]+\\.[0-9]+";
  std::string major_version;
  ASSERT_TRUE(re2::RE2::FullMatch(uploads[0].client_version(),
                                  kChromeVersionRegex, &major_version));
  int major_version_as_interger;
  ASSERT_TRUE(base::StringToInt(major_version, &major_version_as_interger));
  EXPECT_NE(major_version_as_interger, 0);
}

TEST_F(AutofillCrowdsourcingEncoding,
       EncodeUploadRequest_SetsInitialValueChanged) {
  FormData form = test::GetFormData(
      {.fields = {
           // Field 1: Expect `initial_value_changed` not set because the field
           // had no pre-filled value.
           {.role = NAME_FIRST},
           // Field 2: Expect `initial_value_changed == false` because `value`
           // doesn't change.
           {.role = NAME_LAST, .value = u"Doe"},
           // Field 3: Expect `initial_value_changed == true` because `value` is
           // changed (below).
           {.role = EMAIL_ADDRESS, .value = u"test@example.com"},
           // Field 4: Expect `initial_value_changed` not set because the field
           // type resolves to `UNKNOWN_TYPE`.
           {.role = USERNAME, .value = u"username"}}});
  // Form structure preserving the state from page load.
  FormStructure cached_form_structure(form);
  // Form structure containing the state on submit.
  FormStructure form_structure(form);

  cached_form_structure.DetermineHeuristicTypes(GeoIpCountryCode(""), nullptr,
                                                nullptr);

  // Simulate user changed non-pre-filled field value.
  form_structure.field(0)->value = u"John";
  // Simulate user changed pre-filled field value.
  form_structure.field(2)->value = u"changed@example.com";

  // Sets `initial_value_changed` on `form_structure::fields_`.
  form_structure.RetrieveFromCache(
      cached_form_structure,
      FormStructure::RetrieveFromCacheReason::kFormImport);

  const std::vector<AutofillUploadContents> uploads = EncodeUploadRequest(
      form_structure,
      /*available_field_types=*/{}, /*form_was_autofilled=*/false,
      /*login_form_signature=*/"", /*observed_submission=*/true);
  ASSERT_EQ(uploads.size(), 1UL);
  const AutofillUploadContents& upload = uploads[0];

  ASSERT_EQ(upload.field_size(), 4);
  // Field 1.
  EXPECT_FALSE(upload.field(0).has_initial_value_changed());
  // Field 2.
  EXPECT_TRUE(upload.field(1).has_initial_value_changed());
  EXPECT_FALSE(upload.field(1).initial_value_changed());
  // Field 3.
  EXPECT_TRUE(upload.field(2).has_initial_value_changed());
  EXPECT_TRUE(upload.field(2).initial_value_changed());
  // Field 4.
  EXPECT_FALSE(upload.field(3).has_initial_value_changed());
}

// Tests that Autofill does not send votes for a field that was filled with
// fallback.
TEST_F(AutofillCrowdsourcingEncoding,
       EncodeUploadRequest_SkipFieldsFilledWithFallback) {
  FormData form = test::GetFormData({.fields = {{.role = NAME_FIRST}}});
  FormStructure form_structure(form);

  std::vector<AutofillUploadContents> uploads = EncodeUploadRequest(
      form_structure,
      /*available_field_types=*/{}, /*form_was_autofilled=*/false,
      /*login_form_signature=*/"", /*observed_submission=*/true);
  ASSERT_GE(uploads.size(), 1u);
  AutofillUploadContents upload = uploads[0];
  EXPECT_EQ(upload.field_size(), 1);

  // Set the autofilled type of the field as something different from its
  // classified type, representing that the field was filled using this type as
  // fallback.
  form_structure.field(0)->set_autofilled_type(NAME_FULL);
  uploads = EncodeUploadRequest(
      form_structure,
      /*available_field_types=*/{}, /*form_was_autofilled=*/false,
      /*login_form_signature=*/"", /*observed_submission=*/true);
  ASSERT_GE(uploads.size(), 1u);
  upload = uploads[0];
  EXPECT_EQ(upload.field_size(), 0);
}

}  // namespace autofill
