// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/crowdsourcing/autofill_crowdsourcing_encoding.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/containers/to_vector.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/crowdsourcing/randomized_encoder.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/determine_regex_types.h"
#include "components/autofill/core/browser/form_parsing/form_field_parser.h"
#include "components/autofill/core/browser/metrics/log_event.h"
#include "components/autofill/core/browser/proto/api_v1.pb.h"
#include "components/autofill/core/browser/proto/server.pb.h"
#include "components/autofill/core/browser/test_utils/autofill_form_test_utils.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/test_utils/field_prediction_test_matchers.h"
#include "components/autofill/core/common/autofill_debug_features.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/html_field_types.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/version_info/version_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/gurl.h"

namespace autofill {
namespace {

using ::autofill::mojom::SubmissionIndicatorEvent;
using ::autofill::mojom::SubmissionSource;
using ::autofill::test::AddFieldPredictionsToForm;
using ::autofill::test::AddFieldPredictionToForm;
using ::autofill::test::CreateFieldPrediction;
using ::autofill::test::CreateTestFormField;
using ::autofill::test::EqualsPrediction;
using ::base::ASCIIToUTF16;
using ::base::test::EqualsProto;
using ::testing::AllOf;
using ::testing::AnyOf;
using ::testing::Each;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Matcher;
using ::testing::Not;
using ::testing::Optional;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::ResultOf;
using ::testing::UnorderedElementsAre;
using ::version_info::GetProductNameAndVersionForUserAgent;
using FieldPrediction = ::autofill::AutofillQueryResponse::FormSuggestion::
    FieldSuggestion::FieldPrediction;

// Helper struct to specify manual overrides.
struct ManualOverride {
  FormSignature form_signature;
  FieldSignature field_signature;
  const std::vector<FieldType> field_types;
};

// Matcher that does a deep comparison of the AutofillPageQueryRequest protobuf.
// It explicitly compares each proto field using Property matchers to
// provide descriptive error messages in case of a mismatch.
// `serializes_same_as_matcher` at the end is used to check the fields that were
// accidentally missed.
Matcher<AutofillPageQueryRequest> SerializesAndDeepEquals(
    const AutofillPageQueryRequest& expected) {
  auto form_matcher = [](const AutofillPageQueryRequest_Form& expected_form) {
    return AllOf(
        Property("signature", &AutofillPageQueryRequest_Form::signature,
                 expected_form.signature()),
        Property("fields", &AutofillPageQueryRequest_Form::fields,
                 ElementsAreArray(base::ToVector(
                     expected_form.fields(),
                     EqualsProto<AutofillPageQueryRequest_Form_Field>))),
        Property("alternative_signature",
                 &AutofillPageQueryRequest_Form::alternative_signature,
                 expected_form.alternative_signature()));
  };

  std::string expected_string;
  CHECK(expected.SerializeToString(&expected_string));
  auto serializes_same_as_matcher = ResultOf(
      [](const auto& actual) {
        std::string actual_string;
        CHECK(actual.SerializeToString(&actual_string));
        return actual_string;
      },
      Eq(expected_string));

  return AllOf(Property("forms", &AutofillPageQueryRequest::forms,
                        ElementsAreArray(
                            base::ToVector(expected.forms(), form_matcher))),
               Property("experiments", &AutofillPageQueryRequest::experiments,
                        ElementsAreArray(base::ToVector(expected.experiments(),
                                                        Eq<int64_t>))),
               serializes_same_as_matcher);
}

// Matcher that does a deep comparison of the AutofillUploadContents protobuf.
// It explicitly compares each proto field using Property matchers to
// provide descriptive error messages in case of a mismatch.
// `serializes_same_as_matcher` at the end is used to check the fields that were
// accidentally missed.
Matcher<AutofillUploadContents> SerializesAndDeepEquals(
    const AutofillUploadContents& expected) {
  auto strip_metadata = [](AutofillUploadContents upload_content) {
    upload_content.clear_language();
    upload_content.clear_randomized_form_metadata();
    upload_content.clear_three_bit_hashed_form_metadata();
    for (int i = 0; i < upload_content.field_data_size(); ++i) {
      upload_content.mutable_field_data(i)->clear_randomized_field_metadata();
      upload_content.mutable_field_data(i)
          ->clear_three_bit_hashed_field_metadata();
    }
    return upload_content;
  };

#define PROPERTY_EQ(property)                                   \
  Property(#property, &AutofillUploadContents::Field::property, \
           expected.property())
  auto field_matcher = [](const AutofillUploadContents::Field& expected) {
    return AllOf(
        PROPERTY_EQ(signature), PROPERTY_EQ(generation_type),
        PROPERTY_EQ(properties_mask), PROPERTY_EQ(generated_password_changed),
        PROPERTY_EQ(vote_type), PROPERTY_EQ(initial_value_hash),
        PROPERTY_EQ(single_username_vote_type),
        PROPERTY_EQ(is_most_recent_single_username_candidate),
        PROPERTY_EQ(initial_value_changed),
        Property("autofill_type", &AutofillUploadContents::Field::autofill_type,
                 ElementsAreArray(
                     base::ToVector(expected.autofill_type(), Eq<int32_t>))),
        Property("format_string", &AutofillUploadContents::Field::format_string,
                 ElementsAreArray(base::ToVector(expected.format_string(),
                                                 EqualsProto<FormatString>))));
#undef PROPERTY_EQ
  };

  AutofillUploadContents stripped_metadata = strip_metadata(expected);
  std::string expected_string;
  CHECK(stripped_metadata.SerializeToString(&expected_string));
  auto serializes_same_as_matcher = ResultOf(
      [](const auto& actual) {
        std::string actual_string;
        CHECK(actual.SerializeToString(&actual_string));
        return actual_string;
      },
      Eq(expected_string));

#define PROPERTY_EQ(property) \
  Property(#property, &AutofillUploadContents::property, expected.property())
  return AllOf(
      PROPERTY_EQ(client_version), PROPERTY_EQ(form_signature),
      PROPERTY_EQ(structural_form_signature), PROPERTY_EQ(autofill_used),
      PROPERTY_EQ(data_present), PROPERTY_EQ(login_form_signature),
      PROPERTY_EQ(submission), PROPERTY_EQ(passwords_revealed),
      PROPERTY_EQ(password_has_letter),
      PROPERTY_EQ(password_has_special_symbol), PROPERTY_EQ(password_length),
      PROPERTY_EQ(password_special_symbol), PROPERTY_EQ(submission_event),
      PROPERTY_EQ(has_form_tag), PROPERTY_EQ(last_address_form_submitted),
      PROPERTY_EQ(second_last_address_form_submitted),
      PROPERTY_EQ(last_credit_card_form_submitted),
      Property("field_data", &AutofillUploadContents::field_data,
               ElementsAreArray(
                   base::ToVector(expected.field_data(), field_matcher))),
      ResultOf(strip_metadata, serializes_same_as_matcher));
#undef PROPERTY_EQ
}

std::string SerializeAndEncode(const AutofillQueryResponse& response) {
  std::string unencoded_response_string;
  if (!response.SerializeToString(&unencoded_response_string)) {
    LOG(ERROR) << "Cannot serialize the response proto";
    return "";
  }
  return base::Base64Encode(unencoded_response_string);
}

void AddFieldOverrideToForm(
    FormFieldData field_data,
    FieldType field_type,
    AutofillQueryResponse_FormSuggestion* form_suggestion) {
  AddFieldPredictionsToForm(
      field_data,
      {CreateFieldPrediction(field_type, FieldPrediction::SOURCE_OVERRIDE)},
      form_suggestion);
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
// Creates the override specification passed as a parameter to
// `features::debug::kAutofillOverridePredictions`.
std::string CreateManualOverridePrediction(
    const std::vector<ManualOverride>& overrides) {
  std::vector<std::string> override_specs;
  override_specs.reserve(overrides.size());

  for (const auto& override : overrides) {
    std::vector<std::string> spec_pieces;
    spec_pieces.reserve(override.field_types.size() + 2);
    spec_pieces.push_back(
        base::NumberToString(static_cast<uint64_t>(override.form_signature)));
    spec_pieces.push_back(
        base::NumberToString(static_cast<uint32_t>(override.field_signature)));

    for (FieldType type : override.field_types) {
      spec_pieces.push_back(base::NumberToString(static_cast<int>(type)));
    }
    override_specs.push_back(base::JoinString(spec_pieces, "_"));
  }
  return base::JoinString(override_specs, "-");
}
#endif

void ParseRationalizeAndSection(FormStructure& form) {
  const RegexPredictions regex_predictions = DetermineRegexTypes(
      GeoIpCountryCode(""), LanguageCode(""), form.ToFormData(), nullptr);
  regex_predictions.ApplyTo(form.fields());
  form.RationalizeAndAssignSections(GeoIpCountryCode(""), LanguageCode(""),
                                    nullptr);
}

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

TEST_F(AutofillCrowdsourcingEncoding, EncodeUploadRequest) {
  FormData form = test::GetFormData(
      {.fields = {
           {.label = u"First Name",
            .name = u"firstname",
            .form_control_type = FormControlType::kInputText},
           {.label = u"Last Name",
            .name = u"lastname",
            .form_control_type = FormControlType::kInputText},
           {.label = u"Email",
            .name = u"email",
            .form_control_type = FormControlType::kInputEmail},
           {.label = u"Phone",
            .name = u"phone",
            .form_control_type = FormControlType::kInputNumber},
           {.label = u"Country",
            .name = u"country",
            .form_control_type = FormControlType::kSelectOne},
           // Add checkable field.
           {.label = u"Checkable1",
            .name = u"Checkable1",
            .form_control_type = FormControlType::kInputCheckbox},
       }});

  std::vector<FieldTypeSet> possible_field_types;
  test::InitializePossibleTypes(possible_field_types, {NAME_FIRST});
  test::InitializePossibleTypes(possible_field_types, {NAME_LAST});
  test::InitializePossibleTypes(possible_field_types, {EMAIL_ADDRESS});
  test::InitializePossibleTypes(possible_field_types,

                                {PHONE_HOME_WHOLE_NUMBER});
  test::InitializePossibleTypes(possible_field_types,

                                {ADDRESS_HOME_COUNTRY});
  test::InitializePossibleTypes(possible_field_types,

                                {ADDRESS_HOME_COUNTRY});

  std::unique_ptr<FormStructure> form_structure =
      std::make_unique<FormStructure>(form);
  for (const std::unique_ptr<AutofillField>& fs_field : *form_structure) {
    fs_field->set_host_form_signature(form_structure->form_signature());
  }

  ASSERT_EQ(form_structure->field_count(), possible_field_types.size());
  for (size_t i = 0; i < form_structure->field_count(); ++i) {
    form_structure->field(i)->set_possible_types(possible_field_types[i]);
  }

  // Prepare the expected proto string.
  AutofillUploadContents upload;
  upload.set_submission(true);
  upload.set_submission_event(AutofillUploadContents::HTML_FORM_SUBMISSION);
  upload.set_client_version(
      std::string(GetProductNameAndVersionForUserAgent()));
  upload.set_form_signature(form_structure->form_signature().value());
  upload.set_structural_form_signature(
      form_structure->structural_form_signature().value());
  upload.set_autofill_used(false);
  upload.set_data_present("1442000308");
  upload.set_has_form_tag(true);

  test::FillUploadField(upload.add_field_data(), 3763331450U, 3U);
  test::FillUploadField(upload.add_field_data(), 3494530716U, 5U);
  test::FillUploadField(upload.add_field_data(), 1029417091U, 9U);
  test::FillUploadField(upload.add_field_data(), 466116101U, 14U);
  test::FillUploadField(upload.add_field_data(), 2799270304U, 36U);

  EncodeUploadRequestOptions options;
  options.encoder = RandomizedEncoder(
      "seed for testing", AutofillRandomizedValue_EncodingType_ALL_BITS,
      /*anonymous_url_collection_is_enabled=*/true);
  options.available_field_types = {NAME_FIRST,
                                   NAME_LAST,
                                   ADDRESS_HOME_LINE1,
                                   ADDRESS_HOME_LINE2,
                                   ADDRESS_HOME_COUNTRY,
                                   EMAIL_ADDRESS,
                                   PHONE_HOME_WHOLE_NUMBER};
  options.observed_submission = true;
  options.submission_event = SubmissionIndicatorEvent::HTML_FORM_SUBMISSION;

  EXPECT_THAT(EncodeUploadRequest(*form_structure, options),
              ElementsAre(SerializesAndDeepEquals(upload)));

  // Add 2 address fields - this should be still a valid form.
  for (size_t i = 0; i < 2; ++i) {
    test_api(form).Append(
        test::GetFormFieldData({.label = u"Address", .name = u"address"}));
    test::InitializePossibleTypes(possible_field_types,
                                  {ADDRESS_HOME_LINE1, ADDRESS_HOME_LINE2});
  }

  form_structure = std::make_unique<FormStructure>(form);
  ASSERT_EQ(form_structure->field_count(), possible_field_types.size());
  for (size_t i = 0; i < form_structure->field_count(); ++i) {
    form_structure->field(i)->set_host_form_signature(
        form_structure->form_signature());
    form_structure->field(i)->set_possible_types(possible_field_types[i]);
  }

  // Adjust the expected proto string.
  upload.set_form_signature(form_structure->form_signature().value());
  upload.set_structural_form_signature(
      form_structure->structural_form_signature().value());
  upload.set_autofill_used(false);
  upload.set_submission_event(
      AutofillUploadContents_SubmissionIndicatorEvent_HTML_FORM_SUBMISSION);

  // Create an additional 2 fields (total of 7).
  for (int i = 0; i < 2; ++i) {
    test::FillUploadField(upload.add_field_data(), 509334676U, 30U);
  }
  // Put the appropriate autofill type on the different address fields.
  test::FillUploadField(upload.mutable_field_data(5), 509334676U, 31U);
  test::FillUploadField(upload.mutable_field_data(6), 509334676U, 31U);

  EXPECT_THAT(EncodeUploadRequest(*form_structure, options),
              ElementsAre(SerializesAndDeepEquals(upload)));

  // Add 300 address fields - now the form is invalid, as it has too many
  // fields.
  for (size_t i = 0; i < 300; ++i) {
    test_api(form).Append(
        test::GetFormFieldData({.label = u"Address", .name = u"address"}));
    test::InitializePossibleTypes(possible_field_types,
                                  {ADDRESS_HOME_LINE1, ADDRESS_HOME_LINE2});
  }
  form_structure = std::make_unique<FormStructure>(form);
  ASSERT_EQ(form_structure->field_count(), possible_field_types.size());
  for (size_t i = 0; i < form_structure->field_count(); ++i) {
    form_structure->field(i)->set_possible_types(possible_field_types[i]);
  }

  EXPECT_TRUE(EncodeUploadRequest(*form_structure, options).empty());
}

// Tests that EncodeUploadRequest() behaves reasonably in the absence of a
// RandomizedEncoder.
TEST_F(AutofillCrowdsourcingEncoding,
       EncodeUploadRequest_WithOrWithoutEncoder) {
  std::unique_ptr<FormStructure> form_structure;
  FormData form;
  form.set_url(GURL("http://www.foo.com/"));
  form.set_renderer_id(test::MakeFormRendererId());
  form.set_fields({CreateTestFormField("First Name", "firstname", "",
                                       FormControlType::kInputText),
                   CreateTestFormField("Last Name", "lastname", "",
                                       FormControlType::kInputText)});

  form_structure = std::make_unique<FormStructure>(form);

  // Prepare the expected proto.
  AutofillUploadContents upload;
  upload.set_submission(true);
  upload.set_client_version(
      std::string(GetProductNameAndVersionForUserAgent()));
  upload.set_form_signature(form_structure->form_signature().value());
  upload.set_structural_form_signature(
      form_structure->structural_form_signature().value());
  upload.set_autofill_used(false);
  upload.set_data_present("10");
  upload.set_submission_event(
      AutofillUploadContents_SubmissionIndicatorEvent_NONE);
  upload.set_has_form_tag(true);
  {
    AutofillUploadContents::Field* upload_firstname_field =
        upload.add_field_data();
    upload_firstname_field->set_signature(
        *form_structure->field(0)->GetFieldSignature());
  }
  {
    AutofillUploadContents::Field* upload_date_field = upload.add_field_data();
    upload_date_field->set_signature(
        *form_structure->field(1)->GetFieldSignature());
  }

  // Without encoder.
  EncodeUploadRequestOptions options;
  options.encoder = std::nullopt;
  options.available_field_types = {NAME_FIRST};
  options.observed_submission = true;

  EXPECT_THAT(EncodeUploadRequest(*form_structure, options),
              ElementsAre(SerializesAndDeepEquals(upload)));

  // With encoder.
  options.encoder = RandomizedEncoder(
      "seed for testing", AutofillRandomizedValue_EncodingType_ALL_BITS,
      /*anonymous_url_collection_is_enabled=*/true);

  EXPECT_THAT(EncodeUploadRequest(*form_structure, options),
              ElementsAre(SerializesAndDeepEquals(upload)));
}

TEST_F(AutofillCrowdsourcingEncoding, EncodeUploadRequestWithFormatStrings) {
  std::unique_ptr<FormStructure> form_structure;
  FormData form;
  form.set_url(GURL("http://www.foo.com/"));
  form.set_renderer_id(test::MakeFormRendererId());
  form.set_fields(
      {CreateTestFormField("First Name", "firstname", "",
                           FormControlType::kInputText),
       CreateTestFormField("Date", "date", "", FormControlType::kInputText),
       CreateTestFormField("Last four digits of ID", "passport-number", "",
                           FormControlType::kInputText)});

  form_structure = std::make_unique<FormStructure>(form);
  for (auto& fs_field : *form_structure) {
    fs_field->set_host_form_signature(form_structure->form_signature());
  }

  // Prepare the expected proto string.
  AutofillUploadContents upload;
  upload.set_submission(true);
  upload.set_client_version(
      std::string(GetProductNameAndVersionForUserAgent()));
  upload.set_form_signature(form_structure->form_signature().value());
  upload.set_structural_form_signature(
      form_structure->structural_form_signature().value());
  upload.set_autofill_used(false);
  upload.set_data_present("10");
  upload.set_submission_event(
      AutofillUploadContents_SubmissionIndicatorEvent_NONE);
  upload.set_has_form_tag(true);

  {
    AutofillUploadContents::Field* upload_firstname_field =
        upload.add_field_data();
    upload_firstname_field->set_signature(
        *form_structure->field(0)->GetFieldSignature());
  }

  {
    AutofillUploadContents::Field* upload_date_field = upload.add_field_data();
    upload_date_field->set_signature(
        *form_structure->field(1)->GetFieldSignature());
    {
      auto* added_format_string = upload_date_field->add_format_string();
      added_format_string->set_type(FormatString_Type_DATE);
      added_format_string->set_format_string("DD/MM/YYYY");
    }
    {
      auto* added_format_string = upload_date_field->add_format_string();
      added_format_string->set_type(FormatString_Type_DATE);
      added_format_string->set_format_string("MM/DD/YYYY");
    }
  }

  {
    AutofillUploadContents::Field* upload_date_field = upload.add_field_data();
    upload_date_field->set_signature(
        *form_structure->field(2)->GetFieldSignature());
    {
      auto* added_format_string = upload_date_field->add_format_string();
      added_format_string->set_type(FormatString_Type_AFFIX);
      added_format_string->set_format_string("-4");
    }
  }

  EncodeUploadRequestOptions options;
  options.encoder = RandomizedEncoder(
      "seed for testing", AutofillRandomizedValue_EncodingType_ALL_BITS,
      /*anonymous_url_collection_is_enabled=*/true);
  options.fields[form_structure->fields()[1]->global_id()].format_strings = {
      {FormatString_Type_DATE, u"DD/MM/YYYY"},
      {FormatString_Type_DATE, u"MM/DD/YYYY"}};
  options.fields[form_structure->fields()[2]->global_id()].format_strings = {
      {FormatString_Type_AFFIX, u"-4"}};
  options.available_field_types = {NAME_FIRST};
  options.observed_submission = true;

  // TODO(crbug.com/396325496): Also allow forms with empty
  // `available_field_types`.
  EXPECT_THAT(EncodeUploadRequest(*form_structure, options),
              ElementsAre(SerializesAndDeepEquals(upload)));
}

TEST_F(AutofillCrowdsourcingEncoding,
       EncodeUploadRequestWithAdditionalPasswordFormSignature) {
  std::unique_ptr<FormStructure> form_structure;
  std::vector<FieldTypeSet> possible_field_types;
  FormData form;
  form.set_url(GURL("http://www.foo.com/"));
  form.set_renderer_id(test::MakeFormRendererId());
  form.set_fields(
      {CreateTestFormField("First Name", "firstname", "",
                           FormControlType::kInputText, "given-name"),
       CreateTestFormField("Last Name", "lastname", "",
                           FormControlType::kInputText, "family-name"),
       CreateTestFormField("Email", "email", "", FormControlType::kInputEmail,
                           "email"),
       CreateTestFormField("username", "username", "",
                           FormControlType::kInputText, "email"),
       CreateTestFormField("password", "password", "",
                           FormControlType::kInputPassword, "email")});
  test::InitializePossibleTypes(possible_field_types, {NAME_FIRST});
  test::InitializePossibleTypes(possible_field_types, {NAME_LAST});
  test::InitializePossibleTypes(possible_field_types, {EMAIL_ADDRESS});
  test::InitializePossibleTypes(possible_field_types, {USERNAME});
  test::InitializePossibleTypes(possible_field_types,
                                {ACCOUNT_CREATION_PASSWORD});

  form_structure = std::make_unique<FormStructure>(form);
  for (auto& fs_field : *form_structure) {
    fs_field->set_host_form_signature(form_structure->form_signature());
  }

  ASSERT_EQ(form_structure->field_count(), possible_field_types.size());

  EncodeUploadRequestOptions options;
  options.encoder = RandomizedEncoder(
      "seed for testing", AutofillRandomizedValue_EncodingType_ALL_BITS,
      /*anonymous_url_collection_is_enabled=*/true);
  options.available_field_types = {NAME_FIRST, NAME_LAST, EMAIL_ADDRESS,
                                   USERNAME, ACCOUNT_CREATION_PASSWORD};
  options.login_form_signature = FormSignature(42);
  options.observed_submission = true;

  for (size_t i = 0; i < form_structure->field_count(); ++i) {
    form_structure->field(i)->set_possible_types(possible_field_types[i]);

    if (form_structure->field(i)->name() == u"password") {
      auto& field_options =
          options.fields[form_structure->field(i)->global_id()];
      field_options.generation_type = AutofillUploadContents::Field::
          MANUALLY_TRIGGERED_GENERATION_ON_SIGN_UP_FORM;
      field_options.generated_password_changed = true;
    }
    if (form_structure->field(i)->name() == u"username") {
      auto& field_options =
          options.fields[form_structure->field(i)->global_id()];
      field_options.vote_type =
          AutofillUploadContents::Field::CREDENTIALS_REUSED;
    }
  }

  // Prepare the expected proto string.
  AutofillUploadContents upload;
  upload.set_submission(true);
  upload.set_client_version(
      std::string(GetProductNameAndVersionForUserAgent()));
  upload.set_form_signature(form_structure->form_signature().value());
  upload.set_structural_form_signature(
      form_structure->structural_form_signature().value());
  upload.set_autofill_used(false);
  upload.set_data_present("1440000000000000000802");
  upload.set_login_form_signature(42);
  upload.set_submission_event(
      AutofillUploadContents_SubmissionIndicatorEvent_NONE);
  upload.set_has_form_tag(true);

  AutofillUploadContents::Field* upload_firstname_field =
      upload.add_field_data();
  test::FillUploadField(upload_firstname_field,
                        *form_structure->field(0)->GetFieldSignature(), 3U);

  AutofillUploadContents::Field* upload_lastname_field =
      upload.add_field_data();
  test::FillUploadField(upload_lastname_field,
                        *form_structure->field(1)->GetFieldSignature(), 5U);

  AutofillUploadContents::Field* upload_email_field = upload.add_field_data();
  test::FillUploadField(upload_email_field,
                        *form_structure->field(2)->GetFieldSignature(), 9U);

  AutofillUploadContents::Field* upload_username_field =
      upload.add_field_data();
  test::FillUploadField(upload_username_field,
                        *form_structure->field(3)->GetFieldSignature(), 86U);
  upload_username_field->set_vote_type(
      AutofillUploadContents::Field::CREDENTIALS_REUSED);

  AutofillUploadContents::Field* upload_password_field =
      upload.add_field_data();
  test::FillUploadField(upload_password_field,
                        *form_structure->field(4)->GetFieldSignature(), 76U);
  upload_password_field->set_generation_type(
      AutofillUploadContents::Field::
          MANUALLY_TRIGGERED_GENERATION_ON_SIGN_UP_FORM);
  upload_password_field->set_generated_password_changed(true);

  EXPECT_THAT(EncodeUploadRequest(*form_structure, options),
              ElementsAre(SerializesAndDeepEquals(upload)));
}

TEST_F(AutofillCrowdsourcingEncoding, EncodeUploadRequestWithPropertiesMask) {
  std::unique_ptr<FormStructure> form_structure;
  std::vector<FieldTypeSet> possible_field_types;
  FormData form;
  form.set_url(GURL("http://www.foo.com/"));
  form.set_renderer_id(test::MakeFormRendererId());
  form.set_fields({
      [] {
        FormFieldData f =
            CreateTestFormField("First Name", "firstname", "",
                                FormControlType::kInputText, "given-name");
        f.set_name_attribute(f.name());
        f.set_id_attribute(u"first_name");
        f.set_css_classes(u"class1 class2");
        f.set_properties_mask(FieldPropertiesFlags::kHadFocus);
        return f;
      }(),
      [] {
        FormFieldData f =
            CreateTestFormField("Last Name", "lastname", "",
                                FormControlType::kInputText, "family-name");
        f.set_name_attribute(f.name());
        f.set_id_attribute(u"last_name");
        f.set_css_classes(u"class1 class2");
        f.set_properties_mask(FieldPropertiesFlags::kHadFocus |
                              FieldPropertiesFlags::kUserTyped);
        return f;
      }(),
      [] {
        FormFieldData f = CreateTestFormField(
            "Email", "email", "", FormControlType::kInputEmail, "email");
        f.set_name_attribute(f.name());
        f.set_id_attribute(u"e-mail");
        f.set_css_classes(u"class1 class2");
        f.set_properties_mask(FieldPropertiesFlags::kHadFocus |
                              FieldPropertiesFlags::kUserTyped);
        return f;
      }(),
  });
  test::InitializePossibleTypes(possible_field_types, {NAME_FIRST});
  test::InitializePossibleTypes(possible_field_types, {NAME_LAST});
  test::InitializePossibleTypes(possible_field_types, {EMAIL_ADDRESS});

  form_structure = std::make_unique<FormStructure>(form);
  for (auto& fs_field : *form_structure) {
    fs_field->set_host_form_signature(form_structure->form_signature());
  }

  ASSERT_EQ(form_structure->field_count(), possible_field_types.size());

  for (size_t i = 0; i < form_structure->field_count(); ++i) {
    form_structure->field(i)->set_possible_types(possible_field_types[i]);
  }

  // Prepare the expected proto string.
  AutofillUploadContents upload;
  upload.set_submission(true);
  upload.set_client_version(
      std::string(GetProductNameAndVersionForUserAgent()));
  upload.set_form_signature(form_structure->form_signature().value());
  upload.set_structural_form_signature(
      form_structure->structural_form_signature().value());
  upload.set_autofill_used(false);
  upload.set_data_present("1440");
  upload.set_submission_event(
      AutofillUploadContents_SubmissionIndicatorEvent_NONE);
  upload.set_has_form_tag(true);

  test::FillUploadField(upload.add_field_data(), 3763331450U, 3U);
  upload.mutable_field_data(0)->set_properties_mask(
      FieldPropertiesFlags::kHadFocus);
  test::FillUploadField(upload.add_field_data(), 3494530716U, 5U);
  upload.mutable_field_data(1)->set_properties_mask(
      FieldPropertiesFlags::kHadFocus | FieldPropertiesFlags::kUserTyped);
  test::FillUploadField(upload.add_field_data(), 1029417091U, 9U);
  upload.mutable_field_data(2)->set_properties_mask(
      FieldPropertiesFlags::kHadFocus | FieldPropertiesFlags::kUserTyped);

  EncodeUploadRequestOptions options;
  options.encoder = RandomizedEncoder(
      "seed for testing", AutofillRandomizedValue_EncodingType_ALL_BITS,
      /*anonymous_url_collection_is_enabled=*/true);
  options.available_field_types = {NAME_FIRST, NAME_LAST, EMAIL_ADDRESS};
  options.observed_submission = true;

  EXPECT_THAT(EncodeUploadRequest(*form_structure, options),
              ElementsAre(SerializesAndDeepEquals(upload)));
}

TEST_F(AutofillCrowdsourcingEncoding,
       EncodeUploadRequest_ObservedSubmissionFalse) {
  FormData form = test::GetFormData(
      {.fields = {
           {.label = u"First Name",
            .name = u"firstname",
            .form_control_type = FormControlType::kInputText},
           {.label = u"Last Name",
            .name = u"lastname",
            .form_control_type = FormControlType::kInputText},
           {.label = u"Email",
            .name = u"email",
            .form_control_type = FormControlType::kInputEmail},
       }});

  std::vector<FieldTypeSet> possible_field_types;
  test::InitializePossibleTypes(possible_field_types, {NAME_FIRST});
  test::InitializePossibleTypes(possible_field_types, {NAME_LAST});
  test::InitializePossibleTypes(possible_field_types, {EMAIL_ADDRESS});

  std::unique_ptr<FormStructure> form_structure =
      std::make_unique<FormStructure>(form);
  for (const std::unique_ptr<AutofillField>& fs_field : *form_structure) {
    fs_field->set_host_form_signature(form_structure->form_signature());
  }

  ASSERT_EQ(form_structure->field_count(), possible_field_types.size());

  for (size_t i = 0; i < form_structure->field_count(); ++i) {
    form_structure->field(i)->set_possible_types(possible_field_types[i]);
  }

  // Prepare the expected proto string.
  AutofillUploadContents upload;
  upload.set_submission(false);
  upload.set_client_version(
      std::string(GetProductNameAndVersionForUserAgent()));
  upload.set_form_signature(form_structure->form_signature().value());
  upload.set_structural_form_signature(
      form_structure->structural_form_signature().value());
  upload.set_autofill_used(false);
  upload.set_data_present("1440");
  upload.set_submission_event(
      AutofillUploadContents_SubmissionIndicatorEvent_NONE);
  upload.set_has_form_tag(true);

  test::FillUploadField(upload.add_field_data(), 3763331450U, 3U);
  test::FillUploadField(upload.add_field_data(), 3494530716U, 5U);
  test::FillUploadField(upload.add_field_data(), 1029417091U, 9U);

  EncodeUploadRequestOptions options;
  options.encoder = RandomizedEncoder(
      "seed for testing", AutofillRandomizedValue_EncodingType_ALL_BITS,
      /*anonymous_url_collection_is_enabled=*/true);
  options.available_field_types = {NAME_FIRST, NAME_LAST, EMAIL_ADDRESS};
  options.observed_submission = false;

  EXPECT_THAT(EncodeUploadRequest(*form_structure, options),
              ElementsAre(SerializesAndDeepEquals(upload)));
}

TEST_F(AutofillCrowdsourcingEncoding, EncodeUploadRequest_WithLabels) {
  FormData form =
      test::GetFormData({.fields = {
                             // No label for the first field.
                             {.form_control_type = FormControlType::kInputText},
                             {.label = u"Last Name",
                              .form_control_type = FormControlType::kInputText},
                             {.label = u"Email",
                              .form_control_type = FormControlType::kInputText},
                         }});

  std::vector<FieldTypeSet> possible_field_types;
  test::InitializePossibleTypes(possible_field_types, {NAME_FIRST});
  test::InitializePossibleTypes(possible_field_types, {NAME_LAST});
  test::InitializePossibleTypes(possible_field_types, {EMAIL_ADDRESS});

  std::unique_ptr<FormStructure> form_structure =
      std::make_unique<FormStructure>(form);
  for (const std::unique_ptr<AutofillField>& fs_field : *form_structure) {
    fs_field->set_host_form_signature(form_structure->form_signature());
  }

  ASSERT_EQ(form_structure->field_count(), possible_field_types.size());

  for (size_t i = 0; i < form_structure->field_count(); ++i) {
    form_structure->field(i)->set_possible_types(possible_field_types[i]);
  }

  // Prepare the expected proto string.
  AutofillUploadContents upload;
  upload.set_submission(true);
  upload.set_client_version(
      std::string(GetProductNameAndVersionForUserAgent()));
  upload.set_form_signature(form_structure->form_signature().value());
  upload.set_structural_form_signature(
      form_structure->structural_form_signature().value());
  upload.set_autofill_used(false);
  upload.set_data_present("1440");
  upload.set_submission_event(
      AutofillUploadContents_SubmissionIndicatorEvent_NONE);
  upload.set_has_form_tag(true);

  test::FillUploadField(upload.add_field_data(), 1318412689U, 3U);
  test::FillUploadField(upload.add_field_data(), 1318412689U, 5U);
  test::FillUploadField(upload.add_field_data(), 1318412689U, 9U);

  EncodeUploadRequestOptions options;
  options.encoder = RandomizedEncoder(
      "seed for testing", AutofillRandomizedValue_EncodingType_ALL_BITS,
      /*anonymous_url_collection_is_enabled=*/true);
  options.available_field_types = {NAME_FIRST, NAME_LAST, EMAIL_ADDRESS};
  options.observed_submission = true;

  EXPECT_THAT(EncodeUploadRequest(*form_structure, options),
              ElementsAre(SerializesAndDeepEquals(upload)));
}

// Tests that when the form is the result of flattening multiple forms into one,
// EncodeUploadRequest() returns multiple uploads: one for the entire form and
// one for each of the original forms.
TEST_F(AutofillCrowdsourcingEncoding, EncodeUploadRequest_WithSubForms) {
  FormData form =
      test::GetFormData({.fields = {{.host_form_signature = FormSignature(123),
                                     .label = u"Cardholder name",
                                     .name = u"cc-name"},
                                    {.host_frame = test::MakeLocalFrameToken(),
                                     .host_form_signature = FormSignature(456),
                                     .label = u"Credit card number",
                                     .name = u"cc-number"},
                                    {.host_form_signature = FormSignature(123),
                                     .label = u"Expiration date",
                                     .name = u"cc-exp"},
                                    {.host_frame = test::MakeLocalFrameToken(),
                                     .host_form_signature = FormSignature(456),
                                     .label = u"CVC",
                                     .name = u"cc-cvc"}}});

  std::vector<FieldTypeSet> possible_field_types;
  test::InitializePossibleTypes(possible_field_types,

                                {CREDIT_CARD_NAME_FULL});
  test::InitializePossibleTypes(possible_field_types,

                                {CREDIT_CARD_NUMBER});
  test::InitializePossibleTypes(possible_field_types,
                                {CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR});
  test::InitializePossibleTypes(possible_field_types,

                                {CREDIT_CARD_VERIFICATION_CODE});

  ASSERT_EQ(form.global_id(), form.fields()[0].renderer_form_id());
  ASSERT_NE(form.global_id(), form.fields()[1].renderer_form_id());
  ASSERT_EQ(form.global_id(), form.fields()[2].renderer_form_id());
  ASSERT_NE(form.global_id(), form.fields()[3].renderer_form_id());

  std::unique_ptr<FormStructure> form_structure =
      std::make_unique<FormStructure>(form);

  ASSERT_EQ(form_structure->field_count(), possible_field_types.size());
  for (size_t i = 0; i < form_structure->field_count(); ++i) {
    form_structure->field(i)->set_possible_types(possible_field_types[i]);
  }

  // Prepare the expected proto string.
  const AutofillUploadContents upload_main = [&] {
    AutofillUploadContents upload;
    upload.set_submission(true);
    upload.set_submission_event(
        AutofillUploadContents_SubmissionIndicatorEvent_NONE);
    upload.set_client_version(
        std::string(GetProductNameAndVersionForUserAgent()));
    upload.set_form_signature(form_structure->form_signature().value());
    upload.set_structural_form_signature(
        form_structure->structural_form_signature().value());
    upload.set_autofill_used(false);
    upload.set_data_present("0000000000001850");
    upload.set_has_form_tag(true);
    test::FillUploadField(upload.add_field_data(), 3340391946, 51);
    test::FillUploadField(upload.add_field_data(), 1415886167, 52);
    test::FillUploadField(upload.add_field_data(), 3155194603, 57);
    test::FillUploadField(upload.add_field_data(), 917221285, 59);
    return upload;
  }();

  const AutofillUploadContents upload_name_exp = [&] {
    AutofillUploadContents upload;
    upload.set_client_version(
        std::string(GetProductNameAndVersionForUserAgent()));
    upload.set_form_signature(form.fields()[0].host_form_signature().value());
    upload.set_autofill_used(false);
    upload.set_data_present("0000000000001850");
    test::FillUploadField(upload.add_field_data(), 3340391946, 51);
    test::FillUploadField(upload.add_field_data(), 3155194603, 57);
    return upload;
  }();

  const AutofillUploadContents upload_number = [&] {
    AutofillUploadContents upload;
    upload.set_client_version(
        std::string(GetProductNameAndVersionForUserAgent()));
    upload.set_form_signature(form.fields()[1].host_form_signature().value());
    upload.set_autofill_used(false);
    upload.set_data_present("0000000000001850");
    test::FillUploadField(upload.add_field_data(), 1415886167, 52);
    return upload;
  }();

  const AutofillUploadContents upload_cvc = [&] {
    AutofillUploadContents upload;
    upload.set_client_version(
        std::string(GetProductNameAndVersionForUserAgent()));
    upload.set_form_signature(form.fields()[3].host_form_signature().value());
    upload.set_autofill_used(false);
    upload.set_data_present("0000000000001850");
    test::FillUploadField(upload.add_field_data(), 917221285, 59);
    return upload;
  }();

  EncodeUploadRequestOptions options;
  options.encoder = RandomizedEncoder(
      "seed for testing", AutofillRandomizedValue_EncodingType_ALL_BITS,
      /*anonymous_url_collection_is_enabled=*/true);
  options.available_field_types = {CREDIT_CARD_NAME_FULL, CREDIT_CARD_NUMBER,
                                   CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR,
                                   CREDIT_CARD_VERIFICATION_CODE};
  options.observed_submission = true;

  EXPECT_THAT(EncodeUploadRequest(*form_structure, options),
              UnorderedElementsAre(SerializesAndDeepEquals(upload_main),
                                   SerializesAndDeepEquals(upload_name_exp),
                                   SerializesAndDeepEquals(upload_number),
                                   SerializesAndDeepEquals(upload_cvc)));
}

class AutofillCrowdsourcingEncodingUploadProto
    : public AutofillCrowdsourcingEncoding,
      public testing::WithParamInterface<bool> {
 public:
  AutofillCrowdsourcingEncodingUploadProto() = default;
  void SetUp() override {
    feature_list_.InitWithFeatureState(features::kAutofillServerUploadMoreData,
                                       GetParam());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(AutofillCrowdsourcingEncodingUploadProto,
       EncodeUploadRequest_ThreeBitHashedMetadata) {
  const bool kUploadMoreDataEnabled = GetParam();

  FormData form;
  form.set_id_attribute(u"form-id");
  form.set_name_attribute(u"form-name");
  form.set_action(GURL("http://www.foo.com/submit"));
  form.set_button_titles({std::make_pair(
      u"Submit Button", mojom::ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE)});

  FormFieldData field;
  field.set_id_attribute(u"field1-id");
  field.set_name_attribute(u"field1-name");
  field.set_label(u"Field 1 Label");
  field.set_aria_label(u"Field 1 Aria Label");
  field.set_aria_description(u"Field 1 Aria Description");
  field.set_placeholder(u"Field 1 Placeholder");
  field.set_autocomplete_attribute("name");
  field.set_pattern(u"[0-9]*");
  field.set_form_control_type(FormControlType::kInputText);
  field.set_value(u"initial value 1");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  FormStructure form_structure(form);
  EncodeUploadRequestOptions options;

  std::vector<AutofillUploadContents> uploads =
      EncodeUploadRequest(form_structure, options);
  ASSERT_EQ(1u, uploads.size());
  const AutofillUploadContents& upload = uploads.front();

  if (kUploadMoreDataEnabled) {
    // Verify form metadata hashes.
    ASSERT_TRUE(upload.has_three_bit_hashed_form_metadata());
    const ThreeBitHashedFormMetadata& form_metadata =
        upload.three_bit_hashed_form_metadata();
    EXPECT_EQ(form_metadata.id(), StrToHash3Bit(form.id_attribute()));
    EXPECT_EQ(form_metadata.name(), StrToHash3Bit(form.name_attribute()));
    EXPECT_EQ(form_metadata.button_titles_concatenated(),
              StrToHash3Bit(form.button_titles()[0].first));

    // Verify field metadata hashes.
    ASSERT_EQ(upload.field_data_size(), 1);

    const ThreeBitHashedFieldMetadata& field_metadata =
        upload.field_data(0).three_bit_hashed_field_metadata();
    EXPECT_EQ(field_metadata.id(), StrToHash3Bit(field.id_attribute()));
    EXPECT_EQ(field_metadata.name(), StrToHash3Bit(field.name_attribute()));
    EXPECT_EQ(
        field_metadata.type(),
        StrToHash3Bit(FormControlTypeToString(field.form_control_type())));
    EXPECT_EQ(field_metadata.label(), StrToHash3Bit(field.label()));
    EXPECT_EQ(field_metadata.aria_label(), StrToHash3Bit(field.aria_label()));
    EXPECT_EQ(field_metadata.aria_description(),
              StrToHash3Bit(field.aria_description()));
    EXPECT_EQ(field_metadata.placeholder(), StrToHash3Bit(field.placeholder()));
    EXPECT_EQ(field_metadata.initial_value(), StrToHash3Bit(field.value()));
    EXPECT_EQ(field_metadata.autocomplete(),
              StrToHash3Bit(field.autocomplete_attribute()));
    EXPECT_EQ(field_metadata.pattern(), StrToHash3Bit(field.pattern()));
  } else {
    // Verify form metadata hashes are NOT present.
    EXPECT_FALSE(upload.has_three_bit_hashed_form_metadata());

    // Verify field metadata hashes are NOT present.
    ASSERT_EQ(upload.field_data_size(), 1);
    EXPECT_FALSE(upload.field_data(0).has_three_bit_hashed_field_metadata());
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         AutofillCrowdsourcingEncodingUploadProto,
                         testing::Bool());

class AutofillCrowdsourcingEncodingQueryProto
    : public AutofillCrowdsourcingEncoding,
      public testing::WithParamInterface<bool> {
 public:
  AutofillCrowdsourcingEncodingQueryProto() = default;
  void SetUp() override {
    feature_list_.InitWithFeatureState(
        features::kAutofillServerExperimentalSignatures, GetParam());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(AutofillCrowdsourcingEncodingQueryProto,
       EncodeAutofillPageQueryRequest_WithExperimentalSignatures) {
  const bool kExperimentalSignaturesEnabled = GetParam();

  FormData form;
  form.set_id_attribute(u"form-id");
  form.set_name_attribute(u"form-name");
  form.set_url(GURL("http://www.foo.com/"));
  form.set_button_titles({std::make_pair(
      u"Submit Button", mojom::ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE)});

  FormFieldData field;
  field.set_id_attribute(u"field1-id");
  field.set_name_attribute(u"field1-name");
  field.set_label(u"Field 1 Label");
  field.set_aria_label(u"Field 1 Aria Label");
  field.set_aria_description(u"Field 1 Aria Description");
  field.set_placeholder(u"Field 1 Placeholder");
  field.set_autocomplete_attribute("name");
  field.set_pattern(u"[0-9]*");
  field.set_form_control_type(FormControlType::kInputText);
  field.set_value(u"initial value 1");
  field.set_renderer_id(test::MakeFieldRendererId());
  test_api(form).Append(field);

  FormStructure form_structure(form);
  std::vector<raw_ptr<const FormStructure, VectorExperimental>> forms;
  forms.push_back(&form_structure);

  auto [encoded_query, encoded_signatures] =
      EncodeAutofillPageQueryRequest(forms);

  ASSERT_EQ(encoded_query.forms_size(), 1);
  const auto& query_form = encoded_query.forms(0);

  if (kExperimentalSignaturesEnabled) {
    EXPECT_EQ(query_form.structural_signature(),
              form_structure.structural_form_signature().value());

    // Verify form metadata hashes.
    ASSERT_TRUE(query_form.has_three_bit_hashed_form_metadata());
    const auto& form_metadata = query_form.three_bit_hashed_form_metadata();
    EXPECT_EQ(form_metadata.id(), StrToHash3Bit(form.id_attribute()));
    EXPECT_EQ(form_metadata.name(), StrToHash3Bit(form.name_attribute()));
    EXPECT_EQ(form_metadata.button_titles_concatenated(),
              StrToHash3Bit(form.button_titles()[0].first));

    // Verify field metadata hashes.
    ASSERT_EQ(query_form.fields_size(), 1);
    const auto& query_field = query_form.fields(0);
    ASSERT_TRUE(query_field.has_three_bit_hashed_field_metadata());
    const auto& field_metadata = query_field.three_bit_hashed_field_metadata();
    EXPECT_EQ(field_metadata.id(), StrToHash3Bit(field.id_attribute()));
    EXPECT_EQ(field_metadata.name(), StrToHash3Bit(field.name_attribute()));
    EXPECT_EQ(
        field_metadata.type(),
        StrToHash3Bit(FormControlTypeToString(field.form_control_type())));
    EXPECT_EQ(field_metadata.label(), StrToHash3Bit(field.label()));
    EXPECT_EQ(field_metadata.aria_label(), StrToHash3Bit(field.aria_label()));
    EXPECT_EQ(field_metadata.aria_description(),
              StrToHash3Bit(field.aria_description()));
    EXPECT_EQ(field_metadata.placeholder(), StrToHash3Bit(field.placeholder()));
    EXPECT_EQ(field_metadata.initial_value(), StrToHash3Bit(field.value()));
    EXPECT_EQ(field_metadata.autocomplete(),
              StrToHash3Bit(field.autocomplete_attribute()));
    EXPECT_EQ(field_metadata.pattern(), StrToHash3Bit(field.pattern()));
  } else {
    EXPECT_FALSE(query_form.has_structural_signature());
    EXPECT_FALSE(query_form.has_three_bit_hashed_form_metadata());
    ASSERT_EQ(query_form.fields_size(), 1);
    const auto& query_field = query_form.fields(0);
    EXPECT_FALSE(query_field.has_three_bit_hashed_field_metadata());
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         AutofillCrowdsourcingEncodingQueryProto,
                         testing::Bool());

// Check that we compute the "datapresent" string correctly for the given
// |available_types|.
TEST_F(AutofillCrowdsourcingEncoding, CheckDataPresence) {
  FormData form =
      test::GetFormData({.fields = {
                             {.label = u"First Name", .name = u"first"},
                             {.label = u"Last Name", .name = u"last"},
                             {.label = u"Email", .name = u"email"},
                         }});
  FormStructure form_structure(form);
  form_structure.set_submission_source(SubmissionSource::FORM_SUBMISSION);
  for (auto& fs_field : form_structure) {
    fs_field->set_host_form_signature(form_structure.form_signature());
  }

  std::vector<FieldTypeSet> possible_field_types;

  for (size_t i = 0; i < form_structure.field_count(); ++i) {
    test::InitializePossibleTypes(possible_field_types, {UNKNOWN_TYPE});
    form_structure.field(i)->set_possible_types(possible_field_types[i]);
  }

  // Prepare the expected proto string.
  AutofillUploadContents upload;
  upload.set_submission(true);
  upload.set_client_version(
      std::string(GetProductNameAndVersionForUserAgent()));
  upload.set_form_signature(form_structure.form_signature().value());
  upload.set_structural_form_signature(
      form_structure.structural_form_signature().value());
  upload.set_autofill_used(false);
  upload.set_data_present("");
  upload.set_submission_event(
      AutofillUploadContents_SubmissionIndicatorEvent_HTML_FORM_SUBMISSION);
  upload.set_has_form_tag(true);

  test::FillUploadField(upload.add_field_data(), 1089846351U, 1U);
  test::FillUploadField(upload.add_field_data(), 2404144663U, 1U);
  test::FillUploadField(upload.add_field_data(), 420638584U, 1U);

  // No available types.
  // datapresent should be "" == trimmed(0x0000000000000000) ==
  //     0b0000000000000000000000000000000000000000000000000000000000000000
  EncodeUploadRequestOptions options;
  options.encoder = RandomizedEncoder(
      "seed for testing", AutofillRandomizedValue_EncodingType_ALL_BITS,
      /*anonymous_url_collection_is_enabled=*/true);
  options.observed_submission = true;

  EXPECT_THAT(EncodeUploadRequest(form_structure, options),
              ElementsAre(SerializesAndDeepEquals(upload)));

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
  options.available_field_types = {NAME_FIRST,         NAME_LAST,
                                   NAME_FULL,          EMAIL_ADDRESS,
                                   ADDRESS_HOME_LINE1, ADDRESS_HOME_CITY};

  // Adjust the expected proto string.
  upload.set_data_present("1540000240");

  EXPECT_THAT(EncodeUploadRequest(form_structure, options),
              ElementsAre(SerializesAndDeepEquals(upload)));

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
  options.available_field_types = {NAME_FIRST,
                                   NAME_MIDDLE,
                                   NAME_LAST,
                                   NAME_MIDDLE_INITIAL,
                                   NAME_FULL,
                                   EMAIL_ADDRESS,
                                   PHONE_HOME_NUMBER,
                                   PHONE_HOME_CITY_CODE,
                                   PHONE_HOME_COUNTRY_CODE,
                                   PHONE_HOME_CITY_AND_NUMBER,
                                   PHONE_HOME_WHOLE_NUMBER,
                                   ADDRESS_HOME_LINE1,
                                   ADDRESS_HOME_LINE2,
                                   ADDRESS_HOME_CITY,
                                   ADDRESS_HOME_STATE,
                                   ADDRESS_HOME_ZIP,
                                   ADDRESS_HOME_COUNTRY,
                                   COMPANY_NAME};

  // Adjust the expected proto string.
  upload.set_data_present("1f7e000378000008");

  EXPECT_THAT(EncodeUploadRequest(form_structure, options),
              ElementsAre(SerializesAndDeepEquals(upload)));

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
  options.available_field_types = {CREDIT_CARD_NAME_FULL,
                                   CREDIT_CARD_NUMBER,
                                   CREDIT_CARD_EXP_MONTH,
                                   CREDIT_CARD_EXP_2_DIGIT_YEAR,
                                   CREDIT_CARD_EXP_4_DIGIT_YEAR,
                                   CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR,
                                   CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR};

  // Adjust the expected proto string.
  upload.set_data_present("0000000000001fc0");

  EXPECT_THAT(EncodeUploadRequest(form_structure, options),
              ElementsAre(SerializesAndDeepEquals(upload)));

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
  options.available_field_types = {NAME_FIRST,
                                   NAME_MIDDLE,
                                   NAME_LAST,
                                   NAME_MIDDLE_INITIAL,
                                   NAME_FULL,
                                   EMAIL_ADDRESS,
                                   PHONE_HOME_NUMBER,
                                   PHONE_HOME_CITY_CODE,
                                   PHONE_HOME_COUNTRY_CODE,
                                   PHONE_HOME_CITY_AND_NUMBER,
                                   PHONE_HOME_WHOLE_NUMBER,
                                   ADDRESS_HOME_LINE1,
                                   ADDRESS_HOME_LINE2,
                                   ADDRESS_HOME_CITY,
                                   ADDRESS_HOME_STATE,
                                   ADDRESS_HOME_ZIP,
                                   ADDRESS_HOME_COUNTRY,
                                   CREDIT_CARD_NAME_FULL,
                                   CREDIT_CARD_NUMBER,
                                   CREDIT_CARD_EXP_MONTH,
                                   CREDIT_CARD_EXP_2_DIGIT_YEAR,
                                   CREDIT_CARD_EXP_4_DIGIT_YEAR,
                                   CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR,
                                   CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR,
                                   COMPANY_NAME};

  // Adjust the expected proto string.
  upload.set_data_present("1f7e000378001fc8");

  EXPECT_THAT(EncodeUploadRequest(form_structure, options),
              ElementsAre(SerializesAndDeepEquals(upload)));
}

TEST_F(AutofillCrowdsourcingEncoding, CheckMultipleTypes) {
  EncodeUploadRequestOptions options;
  options.encoder = RandomizedEncoder(
      "seed for testing", AutofillRandomizedValue_EncodingType_ALL_BITS,
      /*anonymous_url_collection_is_enabled=*/true);
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
  options.available_field_types = {NAME_FIRST,         NAME_LAST,
                                   EMAIL_ADDRESS,      ADDRESS_HOME_LINE1,
                                   ADDRESS_HOME_LINE2, ADDRESS_HOME_CITY,
                                   ADDRESS_HOME_STATE, COMPANY_NAME};
  options.observed_submission = true;

  // Check that multiple types for the field are processed correctly.
  std::vector<FieldTypeSet> possible_field_types;

  FormData form =
      test::GetFormData({.fields = {{.label = u"email", .name = u"email"},
                                    {.label = u"First Name", .name = u"first"},
                                    {.label = u"Last Name", .name = u"last"},
                                    {.label = u"Address", .name = u"address"}},
                         .renderer_id = FormRendererId()});
  test::InitializePossibleTypes(possible_field_types, {EMAIL_ADDRESS});
  test::InitializePossibleTypes(possible_field_types, {NAME_FIRST});
  test::InitializePossibleTypes(possible_field_types, {NAME_LAST});
  test::InitializePossibleTypes(possible_field_types,

                                {ADDRESS_HOME_LINE1});

  auto form_structure = std::make_unique<FormStructure>(form);
  form_structure->set_submission_source(SubmissionSource::XHR_SUCCEEDED);
  for (size_t i = 0; i < form_structure->field_count(); ++i) {
    form_structure->field(i)->set_host_form_signature(
        form_structure->form_signature());
    form_structure->field(i)->set_possible_types(possible_field_types[i]);
  }

  // Prepare the expected proto string.
  AutofillUploadContents upload;
  upload.set_submission(true);
  upload.set_client_version(
      std::string(GetProductNameAndVersionForUserAgent()));
  upload.set_form_signature(form_structure->form_signature().value());
  upload.set_structural_form_signature(
      form_structure->structural_form_signature().value());
  upload.set_autofill_used(false);
  upload.set_data_present("1440000360000008");
  upload.set_has_form_tag(false);
  upload.set_submission_event(
      AutofillUploadContents_SubmissionIndicatorEvent_XHR_SUCCEEDED);

  test::FillUploadField(upload.add_field_data(), 420638584U, 9U);
  test::FillUploadField(upload.add_field_data(), 1089846351U, 3U);
  test::FillUploadField(upload.add_field_data(), 2404144663U, 5U);
  test::FillUploadField(upload.add_field_data(), 509334676U, 30U);

  EXPECT_THAT(EncodeUploadRequest(*form_structure, options),
              ElementsAre(SerializesAndDeepEquals(upload)));

  // Match third field as both first and last.
  possible_field_types[2].insert(NAME_FIRST);
  form_structure->field(2)->set_possible_types(possible_field_types[2]);

  // Modify the expected upload.
  // Add the NAME_FIRST prediction to the third field.
  test::FillUploadField(upload.mutable_field_data(2), 2404144663U, 3U);

  upload.mutable_field_data(2)->mutable_autofill_type()->SwapElements(0, 1);

  EXPECT_THAT(EncodeUploadRequest(*form_structure, options),
              ElementsAre(SerializesAndDeepEquals(upload)));

  // Match last field as both address home line 1 and 2.
  possible_field_types[3].insert(ADDRESS_HOME_LINE2);
  form_structure->field(form_structure->field_count() - 1)
      ->set_possible_types(
          possible_field_types[form_structure->field_count() - 1]);

  // Adjust the expected upload proto.
  test::FillUploadField(upload.mutable_field_data(3), 509334676U, 31U);

  EXPECT_THAT(EncodeUploadRequest(*form_structure, options),
              ElementsAre(SerializesAndDeepEquals(upload)));

  // Replace the address line 2 prediction by company name.
  possible_field_types[3].clear();
  possible_field_types[3].insert(ADDRESS_HOME_LINE1);
  possible_field_types[3].insert(COMPANY_NAME);
  form_structure->field(form_structure->field_count() - 1)
      ->set_possible_types(
          possible_field_types[form_structure->field_count() - 1]);

  // Adjust the expected upload proto.
  upload.mutable_field_data(3)->set_autofill_type(1, 60);

  EXPECT_THAT(EncodeUploadRequest(*form_structure, options),
              ElementsAre(SerializesAndDeepEquals(upload)));
}

TEST_F(AutofillCrowdsourcingEncoding, EncodeUploadRequest_PasswordsRevealed) {
  // Add 3 fields, to make the form uploadable.
  FormData form = test::GetFormData({.fields = {
                                         {.name = u"email"},
                                         {.name = u"first"},
                                         {.name = u"last"},
                                     }});

  FormStructure form_structure(form);
  for (auto& fs_field : form_structure) {
    fs_field->set_host_form_signature(form_structure.form_signature());
  }

  EncodeUploadRequestOptions options;
  options.encoder = RandomizedEncoder(
      "seed for testing", AutofillRandomizedValue_EncodingType_ALL_BITS,
      /*anonymous_url_collection_is_enabled=*/true);
  options.available_field_types = {NO_SERVER_DATA};
  options.observed_submission = true;

  std::vector<AutofillUploadContents> uploads =
      EncodeUploadRequest(form_structure, options);
  ASSERT_EQ(1u, uploads.size());
}

TEST_F(AutofillCrowdsourcingEncoding, EncodeUploadRequest_IsFormTag) {
  for (bool is_form_tag : {false, true}) {
    FormData form = test::GetFormData(
        {.fields = {{.name = u"email"}},
         .renderer_id =
             is_form_tag ? test::MakeFormRendererId() : FormRendererId()});

    FormStructure form_structure(form);
    for (auto& fs_field : form_structure) {
      fs_field->set_host_form_signature(form_structure.form_signature());
    }
    EncodeUploadRequestOptions options;
    options.encoder = RandomizedEncoder(
        "seed for testing", AutofillRandomizedValue_EncodingType_ALL_BITS,
        /*anonymous_url_collection_is_enabled=*/true);
    options.available_field_types = {NO_SERVER_DATA};
    options.observed_submission = true;

    std::vector<AutofillUploadContents> uploads =
        EncodeUploadRequest(form_structure, options);

    ASSERT_EQ(1u, uploads.size());
    EXPECT_EQ(is_form_tag, uploads.front().has_form_tag());
  }
}

TEST_F(AutofillCrowdsourcingEncoding, EncodeUploadRequest_RichMetadata) {
  struct FieldMetadata {
    const char *id, *name, *label, *placeholder, *aria_label, *aria_description,
        *css_classes, *autocomplete;
    const size_t max_length;
    const std::vector<SelectOption> options;
  };

  static const FieldMetadata kFieldMetadata[] = {
      {"fname_id",
       "fname_name",
       "First Name:",
       "Please enter your first name",
       "Type your first name",
       "You can type your first name here",
       "blah",
       "given-name",
       0,
       {}},
      {"lname_id",
       "lname_name",
       "Last Name:",
       "Please enter your last name",
       "Type your lat name",
       "You can type your last name here",
       "blah",
       "family-name",
       0,
       {}},
      {"email_id",
       "email_name",
       "Email:",
       "Please enter your email address",
       "Type your email address",
       "You can type your email address here",
       "blah",
       "email",
       0,
       {}},
      {"id_only", "", "", "", "", "", "", "", 0, {}},
      {"",
       "name_only",
       "",
       "",
       "",
       "",
       "",
       "",
       FormFieldData::kDefaultMaxLength,
       {}},
      {"date1", "date1", "Year", "", "", "", "", "", 4, {}},
      {"date2", "date2", "Month", "Month", "", "", "", "", 2, {}},
      {"month",
       "",
       "",
       "",
       "",
       "",
       "",
       "",
       0,
       {SelectOption{.value = u"0", .text = u"Select month"},
        SelectOption{.value = u"1", .text = u"January"},
        SelectOption{.value = u"2", .text = u"February"},
        SelectOption{.value = u"12", .text = u"December"}}},
      {"gender",
       "",
       "",
       "",
       "",
       "",
       "",
       "",
       0,
       {SelectOption{.text = u"male"}, SelectOption{.value = u"female"}}},
      {"silly-select",
       "",
       "",
       "",
       "",
       "",
       "",
       "",
       0,
       {SelectOption{.text = u"you get no choice"}}},
      {"silly-select-2",
       "",
       "",
       "",
       "",
       "",
       "",
       "",
       0,
       {SelectOption{.value = u"we are the same",
                     .text = u"we are the same"}}}};

  FormData form;
  form.set_id_attribute(u"form-id");
  form.set_url(GURL("http://www.foo.com/"));
  form.set_button_titles({std::make_pair(
      u"Submit", mojom::ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE)});
  form.set_full_url(GURL("http://www.foo.com/?foo=bar"));
  for (const auto& f : kFieldMetadata) {
    FormFieldData field;
    field.set_id_attribute(ASCIIToUTF16(f.id));
    field.set_name_attribute(ASCIIToUTF16(f.name));
    field.set_name(field.name_attribute());
    field.set_label(ASCIIToUTF16(f.label));
    field.set_placeholder(ASCIIToUTF16(f.placeholder));
    field.set_aria_label(ASCIIToUTF16(f.aria_label));
    field.set_aria_description(ASCIIToUTF16(f.aria_description));
    field.set_css_classes(ASCIIToUTF16(f.css_classes));
    field.set_autocomplete_attribute(f.autocomplete);
    field.set_parsed_autocomplete(ParseAutocompleteAttribute(f.autocomplete));
    field.set_renderer_id(test::MakeFieldRendererId());
    field.set_max_length(f.max_length);
    field.set_options(f.options);
    if (!f.options.empty()) {
      field.set_form_control_type(FormControlType::kSelectOne);
    }
    test_api(form).Append(field);
  }

  FormStructure form_structure(form);
  for (auto& field : form_structure) {
    field->set_host_form_signature(form_structure.form_signature());
  }

  EncodeUploadRequestOptions options;
  options.encoder = RandomizedEncoder(
      "seed for testing", AutofillRandomizedValue_EncodingType_ALL_BITS,
      /*anonymous_url_collection_is_enabled=*/true);
  options.available_field_types = {NO_SERVER_DATA};
  options.observed_submission = true;

  std::vector<AutofillUploadContents> uploads =
      EncodeUploadRequest(form_structure, options);
  ASSERT_EQ(1u, uploads.size());
  AutofillUploadContents& upload = uploads.front();

  const auto form_signature = form_structure.form_signature();

  if (form.id_attribute().empty()) {
    EXPECT_FALSE(upload.randomized_form_metadata().has_id());
  } else {
    EXPECT_EQ(upload.randomized_form_metadata().id().encoded_bits(),
              options.encoder->EncodeForTesting(
                  form_signature, FieldSignature(), RandomizedEncoder::kFormId,
                  form_structure.id_attribute()));
  }

  if (form.name_attribute().empty()) {
    EXPECT_FALSE(upload.randomized_form_metadata().has_name());
  } else {
    EXPECT_EQ(
        upload.randomized_form_metadata().name().encoded_bits(),
        options.encoder->EncodeForTesting(form_signature, FieldSignature(),
                                          RandomizedEncoder::kFormName,
                                          form_structure.name_attribute()));
  }

  auto full_url = form_structure.full_source_url().spec();
  EXPECT_EQ(upload.randomized_form_metadata().url().encoded_bits(),
            options.encoder->Encode(form_signature, FieldSignature(),
                                    RandomizedEncoder::kFormUrl, full_url));
  ASSERT_EQ(static_cast<size_t>(upload.field_data_size()),
            std::size(kFieldMetadata));

  ASSERT_EQ(1, upload.randomized_form_metadata().button_title().size());
  EXPECT_EQ(
      upload.randomized_form_metadata()
          .button_title()[0]
          .title()
          .encoded_bits(),
      options.encoder->EncodeForTesting(form_signature, FieldSignature(),
                                        RandomizedEncoder::kFormButtonTitles,
                                        form.button_titles()[0].first));
  EXPECT_EQ(ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE,
            upload.randomized_form_metadata().button_title()[0].type());

  for (int i = 0; i < upload.field_data_size(); ++i) {
    SCOPED_TRACE(testing::Message() << "field with index " << i);
    const auto& metadata = upload.field_data(i).randomized_field_metadata();
    const auto& field = *form_structure.field(i);
    const auto field_signature = field.GetFieldSignature();
    if (field.id_attribute().empty()) {
      EXPECT_FALSE(metadata.has_id());
    } else {
      EXPECT_EQ(metadata.id().encoded_bits(),
                options.encoder->EncodeForTesting(
                    form_signature, field_signature,
                    RandomizedEncoder::kFieldId, field.id_attribute()));
    }
    if (field.name().empty()) {
      EXPECT_FALSE(metadata.has_name());
    } else {
      EXPECT_EQ(metadata.name().encoded_bits(),
                options.encoder->EncodeForTesting(
                    form_signature, field_signature,
                    RandomizedEncoder::kFieldName, field.name_attribute()));
    }
    EXPECT_EQ(metadata.type().encoded_bits(),
              options.encoder->Encode(
                  form_signature, field_signature,
                  RandomizedEncoder::kFieldControlType,
                  FormControlTypeToString(field.form_control_type())));
    if (field.label().empty()) {
      EXPECT_FALSE(metadata.has_label());
    } else {
      EXPECT_EQ(metadata.label().encoded_bits(),
                options.encoder->EncodeForTesting(
                    form_signature, field_signature,
                    RandomizedEncoder::kFieldLabel, field.label()));
    }
    if (field.aria_label().empty()) {
      EXPECT_FALSE(metadata.has_aria_label());
    } else {
      EXPECT_EQ(metadata.aria_label().encoded_bits(),
                options.encoder->EncodeForTesting(
                    form_signature, field_signature,
                    RandomizedEncoder::kFieldAriaLabel, field.aria_label()));
    }
    if (field.aria_description().empty()) {
      EXPECT_FALSE(metadata.has_aria_description());
    } else {
      EXPECT_EQ(metadata.aria_description().encoded_bits(),
                options.encoder->EncodeForTesting(
                    form_signature, field_signature,
                    RandomizedEncoder::kFieldAriaDescription,
                    field.aria_description()));
    }
    if (field.css_classes().empty()) {
      EXPECT_FALSE(metadata.has_css_class());
    } else {
      EXPECT_EQ(metadata.css_class().encoded_bits(),
                options.encoder->EncodeForTesting(
                    form_signature, field_signature,
                    RandomizedEncoder::kFieldCssClasses, field.css_classes()));
    }
    if (field.placeholder().empty()) {
      EXPECT_FALSE(metadata.has_placeholder());
    } else {
      EXPECT_EQ(metadata.placeholder().encoded_bits(),
                options.encoder->EncodeForTesting(
                    form_signature, field_signature,
                    RandomizedEncoder::kFieldPlaceholder, field.placeholder()));
    }
    if (field.autocomplete_attribute().empty()) {
      EXPECT_FALSE(metadata.has_autocomplete());
    } else {
      EXPECT_EQ(metadata.autocomplete().encoded_bits(),
                options.encoder->EncodeForTesting(
                    form_signature, field_signature,
                    RandomizedEncoder::kFieldAutocomplete,
                    base::UTF8ToUTF16(field.autocomplete_attribute())));
    }
    if (field.max_length() == 0 ||
        field.max_length() == FormFieldData::kDefaultMaxLength) {
      EXPECT_FALSE(metadata.has_max_length());
    } else {
      EXPECT_EQ(metadata.max_length().encoded_bits(),
                options.encoder->EncodeForTesting(
                    form_signature, field_signature,
                    RandomizedEncoder::kFieldMaxLength,
                    base::NumberToString16((field.max_length()))));
    }
    if (field.options().empty()) {
      EXPECT_EQ(metadata.select_option_size(), 0);
    } else {
      // We never encode more than 3 options.
      ASSERT_EQ(metadata.select_option_size(),
                std::min<int>(field.options().size(), 3));
      auto get_option = [&field](int index) {
        return index < 2 ? field.options()[index] : field.options().back();
      };
      for (int j = 0; j < metadata.select_option_size(); ++j) {
        SCOPED_TRACE(testing::Message() << "select option with index " << j);
        if (get_option(j).text.empty()) {
          EXPECT_FALSE(metadata.select_option(j).has_text());
        } else {
          EXPECT_EQ(metadata.select_option(j).text().encoded_bits(),
                    options.encoder->EncodeForTesting(
                        form_signature, field_signature,
                        RandomizedEncoder::kFieldSelectOptionText,
                        get_option(j).text));
        }
        if (get_option(j).value.empty() ||
            get_option(j).value == get_option(j).text) {
          EXPECT_FALSE(metadata.select_option(j).has_value());
        } else {
          EXPECT_EQ(metadata.select_option(j).value().encoded_bits(),
                    options.encoder->EncodeForTesting(
                        form_signature, field_signature,
                        RandomizedEncoder::kFieldSelectOptionValue,
                        get_option(j).value));
        }
      }
    }
  }
}

TEST_F(AutofillCrowdsourcingEncoding, Metadata_OnlySendFullUrlWithUserConsent) {
  for (bool has_consent : {true, false}) {
    SCOPED_TRACE(testing::Message() << " has_consent=" << has_consent);
    FormData form;
    form.set_id_attribute(u"form-id");
    form.set_url(GURL("http://www.foo.com/"));
    form.set_full_url(GURL("http://www.foo.com/?foo=bar"));

    // One form field needed to be valid form.
    FormFieldData field;
    field.set_form_control_type(FormControlType::kInputText);
    field.set_label(u"email");
    field.set_name(u"email");
    field.set_renderer_id(test::MakeFieldRendererId());
    form.set_fields({field});

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

    EncodeUploadRequestOptions options;
    options.encoder = RandomizedEncoder::Create(&prefs);
    options.observed_submission = true;

    std::vector<AutofillUploadContents> uploads =
        EncodeUploadRequest(form_structure, options);

    EXPECT_EQ(has_consent,
              uploads.front().randomized_form_metadata().has_url());
  }
}

TEST_F(AutofillCrowdsourcingEncoding,
       EncodeUploadRequest_WithSingleUsernameVoteType) {
  FormData form;
  form.set_url(GURL("http://www.foo.com/"));
  FormFieldData field;
  field.set_name(u"text field");
  field.set_renderer_id(test::MakeFieldRendererId());
  form.set_fields({field});

  FormStructure form_structure(form);
  for (auto& fs_field : form_structure) {
    fs_field->set_host_form_signature(form_structure.form_signature());
  }

  EncodeUploadRequestOptions options;
  options.encoder = RandomizedEncoder(
      "seed for testing", AutofillRandomizedValue_EncodingType_ALL_BITS,
      /*anonymous_url_collection_is_enabled=*/true);
  options.available_field_types = {NO_SERVER_DATA};
  options.observed_submission = true;
  options.fields[form_structure.field(0)->global_id()]
      .single_username_vote_type = AutofillUploadContents::Field::STRONG;
  options.fields[form_structure.field(0)->global_id()]
      .is_most_recent_single_username_candidate =
      IsMostRecentSingleUsernameCandidate::kMostRecentCandidate;

  std::vector<AutofillUploadContents> uploads =
      EncodeUploadRequest(form_structure, options);
  ASSERT_EQ(1u, uploads.size());
  EXPECT_EQ(AutofillUploadContents::Field::STRONG,
            uploads.front().field_data(0).single_username_vote_type());
  EXPECT_TRUE(
      uploads.front().field_data(0).is_most_recent_single_username_candidate());
}

// Checks that CreateForPasswordManagerUpload builds FormStructure
// which is encodable (i.e. ready for uploading).
TEST_F(AutofillCrowdsourcingEncoding, CreateForPasswordManagerUpload) {
  std::unique_ptr<FormStructure> form =
      FormStructure::CreateForPasswordManagerUpload(
          FormSignature(1234),
          {FieldSignature(1), FieldSignature(10), FieldSignature(100)});
  for (auto& field : *form) {
    field->set_host_form_signature(form->form_signature());
  }
  EXPECT_EQ(FormSignature(1234u), form->form_signature());
  ASSERT_EQ(3u, form->field_count());
  ASSERT_EQ(FieldSignature(100u), form->field(2)->GetFieldSignature());

  EncodeUploadRequestOptions options;
  options.encoder = RandomizedEncoder(
      "seed for testing", AutofillRandomizedValue_EncodingType_ALL_BITS,
      /*anonymous_url_collection_is_enabled=*/true);
  options.observed_submission = true;

  std::vector<AutofillUploadContents> uploads =
      EncodeUploadRequest(*form, options);
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
    field->set_host_form_signature(form->form_signature());
  }

  EncodeUploadRequestOptions options;
  options.encoder = RandomizedEncoder(
      "seed for testing", AutofillRandomizedValue_EncodingType_ALL_BITS,
      /*anonymous_url_collection_is_enabled=*/true);
  options.observed_submission = true;

  std::vector<AutofillUploadContents> uploads =
      EncodeUploadRequest(*form, options);
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
  FormStructure form_structure(form);
  ParseRationalizeAndSection(form_structure);

  // Simulate user changed non-pre-filled field value.
  form_structure.field(0)->set_value(u"John");
  // Simulate user changed pre-filled field value.
  form_structure.field(2)->set_value(u"changed@example.com");

  EncodeUploadRequestOptions options;
  options.encoder = RandomizedEncoder(
      "seed for testing", AutofillRandomizedValue_EncodingType_ALL_BITS,
      /*anonymous_url_collection_is_enabled=*/true);
  options.observed_submission = true;

  const std::vector<AutofillUploadContents> uploads =
      EncodeUploadRequest(form_structure, options);
  ASSERT_EQ(uploads.size(), 1UL);
  const AutofillUploadContents& upload = uploads[0];

  ASSERT_EQ(upload.field_data_size(), 4);
  // Field 1.
  EXPECT_FALSE(upload.field_data(0).has_initial_value_changed());
  // Field 2.
  EXPECT_TRUE(upload.field_data(1).has_initial_value_changed());
  EXPECT_FALSE(upload.field_data(1).initial_value_changed());
  // Field 3.
  EXPECT_TRUE(upload.field_data(2).has_initial_value_changed());
  EXPECT_TRUE(upload.field_data(2).initial_value_changed());
  // Field 4.
  EXPECT_FALSE(upload.field_data(3).has_initial_value_changed());
}

// Tests that Autofill does send votes for a field that was filled with
// fallback. Chrome clients should upload all form fields, see
// crbug.com/444147005 for more details.
TEST_F(AutofillCrowdsourcingEncoding,
       EncodeUploadRequest_SkipFieldsFilledWithFallback) {
  FormData form = test::GetFormData({.fields = {{.role = NAME_FIRST}}});
  FormStructure form_structure(form);

  EncodeUploadRequestOptions options;
  options.encoder = RandomizedEncoder(
      "seed for testing", AutofillRandomizedValue_EncodingType_ALL_BITS,
      /*anonymous_url_collection_is_enabled=*/true);
  options.observed_submission = true;

  std::vector<AutofillUploadContents> uploads =
      EncodeUploadRequest(form_structure, options);
  ASSERT_GE(uploads.size(), 1u);
  AutofillUploadContents upload = uploads[0];
  EXPECT_EQ(upload.field_data_size(), 1);

  // Set the autofilled type of the field as something different from its
  // classified type, representing that the field was filled using this type as
  // fallback.
  form_structure.field(0)->set_autofilled_type(NAME_FULL);
  uploads = EncodeUploadRequest(form_structure, options);
  ASSERT_GE(uploads.size(), 1u);
  upload = uploads[0];
  EXPECT_EQ(upload.field_data_size(), 1);
}

TEST_F(AutofillCrowdsourcingEncoding, EncodeAutofillPageQueryRequest) {
  FormSignature form_signature(16692857476255362434UL);

  FormData form;
  form.set_url(GURL("http://www.foo.com/"));

  FormFieldData field;
  field.set_form_control_type(FormControlType::kInputText);

  field.set_label(u"Name on Card");
  field.set_name(u"name_on_card");
  field.set_renderer_id(test::MakeFieldRendererId());
  field.set_host_form_signature(form_signature);
  test_api(form).Append(field);

  field.set_label(u"Address");
  field.set_name(u"billing_address");
  field.set_renderer_id(test::MakeFieldRendererId());
  field.set_host_form_signature(FormSignature(12345UL));
  test_api(form).Append(field);

  field.set_label(u"Card Number");
  field.set_name(u"card_number");
  field.set_renderer_id(test::MakeFieldRendererId());
  field.set_host_form_signature(FormSignature(67890UL));
  test_api(form).Append(field);

  field.set_label(u"Expiration Date");
  field.set_name(u"expiration_month");
  field.set_renderer_id(test::MakeFieldRendererId());
  field.set_host_form_signature(FormSignature(12345UL));
  test_api(form).Append(field);

  field.set_label(u"Expiration Year");
  field.set_name(u"expiration_year");
  field.set_renderer_id(test::MakeFieldRendererId());
  field.set_host_form_signature(FormSignature(12345UL));
  test_api(form).Append(field);

  // Add checkable field.
  FormFieldData checkable_field;
  checkable_field.set_form_control_type(FormControlType::kInputCheckbox);
  checkable_field.set_check_status(
      FormFieldData::CheckStatus::kCheckableButUnchecked);
  checkable_field.set_label(u"Checkable1");
  checkable_field.set_name(u"Checkable1");
  checkable_field.set_renderer_id(test::MakeFieldRendererId());
  checkable_field.set_host_form_signature(form_signature);
  test_api(form).Append(checkable_field);

  FormStructure form_structure(form);

  std::vector<raw_ptr<const FormStructure, VectorExperimental>> forms;
  forms.push_back(&form_structure);

  std::vector<FormSignature> expected_signatures;
  expected_signatures.emplace_back(form_signature.value());
  expected_signatures.emplace_back(12345UL);
  expected_signatures.emplace_back(67890UL);

  // Prepare the expected proto string.
  AutofillPageQueryRequest query;
  query.set_client_version(std::string(GetProductNameAndVersionForUserAgent()));
  {
    AutofillPageQueryRequest::Form* query_form = query.add_forms();
    query_form->set_signature(form_signature.value());
    query_form->set_alternative_signature(
        form_structure.alternative_form_signature().value());
    query_form->add_fields()->set_signature(412125936U);
    query_form->add_fields()->set_signature(1917667676U);
    query_form->add_fields()->set_signature(2226358947U);
    query_form->add_fields()->set_signature(747221617U);
    query_form->add_fields()->set_signature(4108155786U);

    query_form = query.add_forms();
    query_form->set_signature(12345UL);
    query_form->set_alternative_signature(
        form_structure.alternative_form_signature().value());
    query_form->add_fields()->set_signature(1917667676U);
    query_form->add_fields()->set_signature(747221617U);
    query_form->add_fields()->set_signature(4108155786U);

    query_form = query.add_forms();
    query_form->set_signature(67890UL);
    query_form->set_alternative_signature(
        form_structure.alternative_form_signature().value());
    query_form->add_fields()->set_signature(2226358947U);
  }

  auto [encoded_query, encoded_signatures] =
      EncodeAutofillPageQueryRequest(forms);
  EXPECT_EQ(encoded_signatures, expected_signatures);
  EXPECT_THAT(encoded_query, SerializesAndDeepEquals(query));

  // Add the same form, only one will be encoded, so
  // EncodeAutofillPageQueryRequest() should return the same data.
  FormStructure form_structure2(form);
  forms.push_back(&form_structure2);

  std::vector<FormSignature> expected_signatures2 = expected_signatures;
  auto [encoded_query2, encoded_signatures2] =
      EncodeAutofillPageQueryRequest(forms);
  EXPECT_EQ(encoded_signatures2, expected_signatures2);
  EXPECT_THAT(encoded_query2, SerializesAndDeepEquals(query));

  // Add 5 address fields - this should be still a valid form.
  FormSignature form_signature3(2608858059775241169UL);
  for (auto& f : test_api(form).fields()) {
    if (f.host_form_signature() == form_signature) {
      f.set_host_form_signature(form_signature3);
    }
  }
  for (size_t i = 0; i < 5; ++i) {
    field.set_label(u"Address");
    field.set_name(u"address");
    field.set_renderer_id(test::MakeFieldRendererId());
    field.set_host_form_signature(form_signature3);
    test_api(form).Append(field);
  }

  FormStructure form_structure3(form);
  forms.push_back(&form_structure3);

  std::vector<FormSignature> expected_signatures3 = expected_signatures2;
  expected_signatures3.push_back(form_signature3);

  // Add the second form to the expected proto.
  {
    AutofillPageQueryRequest::Form* query_form = query.add_forms();
    query_form->set_signature(2608858059775241169);
    query_form->set_alternative_signature(
        form_structure3.alternative_form_signature().value());
    query_form->add_fields()->set_signature(412125936U);
    query_form->add_fields()->set_signature(1917667676U);
    query_form->add_fields()->set_signature(2226358947U);
    query_form->add_fields()->set_signature(747221617U);
    query_form->add_fields()->set_signature(4108155786U);
    for (int i = 0; i < 5; ++i) {
      query_form->add_fields()->set_signature(509334676U);
    }
  }

  auto [encoded_query3, encoded_signatures3] =
      EncodeAutofillPageQueryRequest(forms);
  EXPECT_EQ(encoded_signatures3, expected_signatures3);
  EXPECT_THAT(encoded_query3, SerializesAndDeepEquals(query));

  // |form_structures4| will have the same signature as |form_structure3|.
  test_api(form).field(-1).set_name(u"address123456789");

  FormStructure form_structure4(form);
  forms.push_back(&form_structure4);

  std::vector<FormSignature> expected_signatures4 = expected_signatures3;

  auto [encoded_query4, encoded_signatures4] =
      EncodeAutofillPageQueryRequest(forms);
  EXPECT_EQ(encoded_signatures4, expected_signatures4);
  EXPECT_THAT(encoded_query4, SerializesAndDeepEquals(query));

  FormData malformed_form(form);
  // Add 300 address fields - the form is not valid anymore, but previous ones
  // are. The result should be the same as in previous test.
  for (size_t i = 0; i < 300; ++i) {
    field.set_label(u"Address");
    field.set_name(u"address");
    field.set_renderer_id(test::MakeFieldRendererId());
    test_api(malformed_form).Append(field);
  }

  FormStructure malformed_form_structure(malformed_form);
  forms.push_back(&malformed_form_structure);

  std::vector<FormSignature> expected_signatures5 = expected_signatures4;

  auto [encoded_query5, encoded_signatures5] =
      EncodeAutofillPageQueryRequest(forms);
  EXPECT_EQ(encoded_signatures5, expected_signatures5);
  EXPECT_THAT(encoded_query5, SerializesAndDeepEquals(query));

  // Check that we fail if there are only bad form(s).
  std::vector<raw_ptr<const FormStructure, VectorExperimental>> bad_forms;
  bad_forms.push_back(&malformed_form_structure);
  auto [encoded_query6, encoded_signatures6] =
      EncodeAutofillPageQueryRequest(bad_forms);
  EXPECT_TRUE(encoded_signatures6.empty());
}

TEST_F(AutofillCrowdsourcingEncoding, SkipFieldTest) {
  FormData form = test::GetFormData({
      .fields = {{.role = USERNAME},
                 {.label = u"select",
                  .name = u"select",
                  .form_control_type = FormControlType::kInputCheckbox},
                 {.role = EMAIL_ADDRESS}},
      .name = u"the-name",
      .url = "http://cool.com",
      .action = "http://cool.com/login",
  });

  FormStructure form_structure(form);
  std::vector<raw_ptr<const FormStructure, VectorExperimental>> forms;
  forms.push_back(&form_structure);

  // Create the expected query and serialize it to a string.
  AutofillPageQueryRequest query;
  query.set_client_version(std::string(GetProductNameAndVersionForUserAgent()));
  AutofillPageQueryRequest::Form* query_form = query.add_forms();
  query_form->set_signature(form_structure.form_signature().value());
  query_form->set_alternative_signature(
      form_structure.alternative_form_signature().value());

  query_form->add_fields()->set_signature(239111655U);
  query_form->add_fields()->set_signature(420638584U);

  const FormSignature kExpectedSignature(18006745212084723782UL);

  auto [encoded_query, encoded_signatures] =
      EncodeAutofillPageQueryRequest(forms);
  ASSERT_EQ(1U, encoded_signatures.size());
  EXPECT_EQ(kExpectedSignature, encoded_signatures.front());
  EXPECT_THAT(encoded_query, SerializesAndDeepEquals(query));
}

TEST_F(AutofillCrowdsourcingEncoding,
       EncodeAutofillPageQueryRequest_WithLabels) {
  FormData form = test::GetFormData({
      .fields =
          {// No label on the first field.
           {.name = u"username"},
           {.label = u"Enter your Email address", .name = u"email"},
           {.label = u"Enter your Password",
            .name = u"password",
            .form_control_type = FormControlType::kInputPassword}},
      .name = u"the-name",
      .url = "http://cool.com",
      .action = "http://cool.com/login",
  });

  std::vector<raw_ptr<const FormStructure, VectorExperimental>> forms;
  FormStructure form_structure(form);
  forms.push_back(&form_structure);

  // Create the expected query and serialize it to a string.
  AutofillPageQueryRequest query;
  query.set_client_version(std::string(GetProductNameAndVersionForUserAgent()));
  AutofillPageQueryRequest::Form* query_form = query.add_forms();
  query_form->set_signature(form_structure.form_signature().value());
  query_form->set_alternative_signature(
      form_structure.alternative_form_signature().value());

  query_form->add_fields()->set_signature(239111655U);
  query_form->add_fields()->set_signature(420638584U);
  query_form->add_fields()->set_signature(2051817934U);

  auto [encoded_query, encoded_signatures] =
      EncodeAutofillPageQueryRequest(forms);
  ASSERT_TRUE(!encoded_signatures.empty());
  EXPECT_THAT(encoded_query, SerializesAndDeepEquals(query));
}

TEST_F(AutofillCrowdsourcingEncoding,
       EncodeAutofillPageQueryRequest_WithLongLabels) {
  FormData form = test::GetFormData({
      .fields =
          {// No label on the first field.
           {.name = u"username"},
           // This label will be truncated in the XML request.
           {.label = u"Enter Your Really Really Really (Really!) Long Email "
                     u"Address Which We "
                     u"Hope To Get In Order To Send You Unwanted Publicity "
                     u"Because That's "
                     u"What Marketers Do! We Know That Your Email Address Has "
                     u"The Possibility "
                     u"Of Exceeding A Certain Number Of Characters...",
            .name = u"email"},
           {.label = u"Enter your Password",
            .name = u"password",
            .form_control_type = FormControlType::kInputPassword}},
      .name = u"the-name",
      .url = "http://cool.com",
      .action = "http://cool.com/login",
  });

  FormStructure form_structure(form);
  std::vector<raw_ptr<const FormStructure, VectorExperimental>> forms;
  forms.push_back(&form_structure);

  // Create the expected query and serialize it to a string.
  AutofillPageQueryRequest query;
  query.set_client_version(std::string(GetProductNameAndVersionForUserAgent()));
  AutofillPageQueryRequest::Form* query_form = query.add_forms();
  query_form->set_signature(form_structure.form_signature().value());
  query_form->set_alternative_signature(
      form_structure.alternative_form_signature().value());

  query_form->add_fields()->set_signature(239111655U);
  query_form->add_fields()->set_signature(420638584U);
  query_form->add_fields()->set_signature(2051817934U);

  auto [encoded_query, encoded_signatures] =
      EncodeAutofillPageQueryRequest(forms);
  ASSERT_TRUE(!encoded_signatures.empty());
  EXPECT_THAT(encoded_query, SerializesAndDeepEquals(query));
}

// One name is missing from one field.
TEST_F(AutofillCrowdsourcingEncoding,
       EncodeAutofillPageQueryRequest_MissingNames) {
  FormData form = test::GetFormData({
      .fields = {{.role = USERNAME},
                 // No name set for this field.
                 {.label = u"",
                  .name = u"",
                  .form_control_type = FormControlType::kInputText}},
      // No name set for the form.
      .name = u"",
      .url = "http://cool.com",
      .action = "http://cool.com/login",
  });

  FormStructure form_structure(form);
  for (auto& fs_field : form_structure) {
    fs_field->set_host_form_signature(form_structure.form_signature());
  }

  std::vector<raw_ptr<const FormStructure, VectorExperimental>> forms;
  forms.push_back(&form_structure);

  // Create the expected query and serialize it to a string.
  AutofillPageQueryRequest query;
  query.set_client_version(std::string(GetProductNameAndVersionForUserAgent()));
  AutofillPageQueryRequest::Form* query_form = query.add_forms();
  query_form->set_signature(form_structure.form_signature().value());
  query_form->set_alternative_signature(
      form_structure.alternative_form_signature().value());

  query_form->add_fields()->set_signature(239111655U);
  query_form->add_fields()->set_signature(1318412689U);

  const FormSignature kExpectedSignature(16416961345885087496UL);
  auto [encoded_query, encoded_signatures] =
      EncodeAutofillPageQueryRequest(forms);
  ASSERT_EQ(1U, encoded_signatures.size());
  EXPECT_EQ(kExpectedSignature, encoded_signatures.front());
  EXPECT_THAT(encoded_query, SerializesAndDeepEquals(query));
}

TEST_F(AutofillCrowdsourcingEncoding, AllowBigForms) {
  FormData form;
  form.set_url(GURL("http://foo.com"));
  // Check that the form with 250 fields are processed correctly.
  for (size_t i = 0; i < 250; ++i) {
    test_api(form).Append(test::GetFormFieldData({
        .name = u"text" + base::NumberToString16(i),
    }));
  }

  FormStructure form_structure(form);

  std::vector<raw_ptr<const FormStructure, VectorExperimental>> forms;
  forms.push_back(&form_structure);
  auto [encoded_query, encoded_signatures] =
      EncodeAutofillPageQueryRequest(forms);
  EXPECT_EQ(1u, encoded_signatures.size());
}

// Test that server overrides get precedence over HTML types.
TEST_F(AutofillCrowdsourcingEncoding,
       ParseQueryResponse_ServerPredictionIsOverride) {
  FormData form_data = test::GetFormData(
      {.fields = {// Just some field with an autocomplete attribute.
                  {.label = u"some field",
                   .name = u"some_field",
                   .autocomplete_attribute = "name"},
                  // Some other field with the same autocomplete attribute.
                  {.label = u"some other field",
                   .name = u"some_other_field",
                   .autocomplete_attribute = "name"}}});

  // Setup the query response with an override for the name field to be a first
  // name.
  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  AddFieldOverrideToForm(form_data.fields()[0], NAME_FIRST, form_suggestion);
  AddFieldPredictionToForm(form_data.fields()[1], NAME_LAST, form_suggestion);

  // Parse the response and update the field type predictions.
  FormStructure form(form_data);
  ParseRationalizeAndSection(form);

  std::vector<raw_ptr<FormStructure, VectorExperimental>> forms{&form};
  ParseServerPredictionsQueryResponse(SerializeAndEncode(response), forms,
                                      test::GetEncodedSignatures(forms),
                                      nullptr);
  ASSERT_EQ(form.field_count(), 2U);

  // Validate the type predictions.
  EXPECT_EQ(UNKNOWN_TYPE, form.field(0)->heuristic_type());
  EXPECT_EQ(HtmlFieldType::kName, form.field(0)->html_type());
  EXPECT_EQ(NAME_FIRST, form.field(0)->server_type());
  EXPECT_EQ(UNKNOWN_TYPE, form.field(1)->heuristic_type());
  EXPECT_EQ(HtmlFieldType::kName, form.field(1)->html_type());
  EXPECT_EQ(NAME_LAST, form.field(1)->server_type());

  // Validate that the overrides are set correctly.
  EXPECT_TRUE(form.field(0)->server_type_prediction_is_override());
  EXPECT_FALSE(form.field(1)->server_type_prediction_is_override());

  // Validate that the server prediction won for the first field.
  EXPECT_THAT(form.field(0)->Type().GetTypes(), ElementsAre(NAME_FIRST));
  EXPECT_THAT(form.field(1)->Type().GetTypes(), ElementsAre(NAME_FULL));

  // Validate that the server override cannot be altered.
  form.field(0)->SetTypeTo(AutofillType(NAME_FULL),
                           AutofillPredictionSource::kHeuristics);
  EXPECT_THAT(form.field(0)->Type().GetTypes(), ElementsAre(NAME_FIRST));

  // Validate that that the non-override can be altered.
  form.field(1)->SetTypeTo(AutofillType(NAME_FIRST),
                           AutofillPredictionSource::kHeuristics);
  EXPECT_THAT(form.field(1)->Type().GetTypes(), ElementsAre(NAME_FIRST));
}

// Test the heuristic prediction for NAME_LAST_SECOND overrides server
// predictions.
TEST_F(AutofillCrowdsourcingEncoding,
       ParseQueryResponse_HeuristicsOverrideSpanishLastNameTypes) {
  FormData form_data = test::GetFormData(
      {.fields =
           {// First name field.
            {.label = u"Nombre", .name = u"Nombre"},
            // First last name field.
            // Should be identified by local heuristics.
            {.label = u"Apellido Paterno", .name = u"apellido_paterno"},
            // Second last name field.
            // Should be identified by local heuristics.
            {.label = u"Apellido Materno", .name = u"apellido materno"}},
       .url = "http://foo.com"});

  FormStructure form(form_data);
  ParseRationalizeAndSection(form);

  // Setup the query response.
  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  AddFieldPredictionToForm(form_data.fields()[0], NAME_FIRST, form_suggestion);
  // Simulate a NAME_LAST classification for the two last name fields.
  AddFieldPredictionToForm(form_data.fields()[1], NAME_LAST, form_suggestion);
  AddFieldPredictionToForm(form_data.fields()[2], NAME_LAST, form_suggestion);

  // Parse the response and update the field type predictions.
  std::vector<raw_ptr<FormStructure, VectorExperimental>> forms{&form};
  ParseServerPredictionsQueryResponse(SerializeAndEncode(response), forms,
                                      test::GetEncodedSignatures(forms),
                                      nullptr);
  ASSERT_EQ(form.field_count(), 3U);

  // Validate the heuristic and server predictions.
  EXPECT_EQ(NAME_LAST_FIRST, form.field(1)->heuristic_type());
  EXPECT_EQ(NAME_LAST_SECOND, form.field(2)->heuristic_type());
  EXPECT_EQ(NAME_LAST, form.field(1)->server_type());
  EXPECT_EQ(NAME_LAST, form.field(2)->server_type());

  // Validate that the heuristic prediction wins for the two last name fields.
  EXPECT_THAT(form.field(0)->Type().GetTypes(), ElementsAre(NAME_FIRST));
  EXPECT_THAT(form.field(1)->Type().GetTypes(), ElementsAre(NAME_LAST_FIRST));
  EXPECT_THAT(form.field(2)->Type().GetTypes(), ElementsAre(NAME_LAST_SECOND));
}

// Test the heuristic prediction for ADDRESS_HOME_STREET_NAME and
// ADDRESS_HOME_HOUSE_NUMBER overrides server predictions.
TEST_F(AutofillCrowdsourcingEncoding,
       ParseQueryResponse_HeuristicsOverrideStreetNameAndHouseNumberTypes) {
  FormData form_data = test::GetFormData(
      {.fields =
           {// Field for the name.
            {.label = u"Name", .name = u"Name"},
            // Field for the street name.
            {.label = u"Street Name", .name = u"street_name"},
            // Field for the house number.
            {.label = u"House Number", .name = u"house_number"},
            // Field for the postal code.
            {.label = u"ZIP", .name = u"ZIP"}},
       .url = "http://foo.com"});

  FormStructure form(form_data);
  ParseRationalizeAndSection(form);

  // Setup the query response.
  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  AddFieldPredictionToForm(form_data.fields()[0], NAME_FULL, form_suggestion);
  // Simulate ADDRESS_LINE classifications for the two last name fields.
  AddFieldPredictionToForm(form_data.fields()[1], ADDRESS_HOME_LINE1,
                           form_suggestion);
  AddFieldPredictionToForm(form_data.fields()[2], ADDRESS_HOME_LINE2,
                           form_suggestion);

  // Parse the response and update the field type predictions.
  std::vector<raw_ptr<FormStructure, VectorExperimental>> forms{&form};
  ParseServerPredictionsQueryResponse(SerializeAndEncode(response), forms,
                                      test::GetEncodedSignatures(forms),
                                      nullptr);
  ASSERT_EQ(form.field_count(), 4U);

  // Validate the heuristic and server predictions.
  EXPECT_EQ(ADDRESS_HOME_STREET_NAME, form.field(1)->heuristic_type());
  EXPECT_EQ(ADDRESS_HOME_HOUSE_NUMBER, form.field(2)->heuristic_type());
  EXPECT_EQ(ADDRESS_HOME_LINE1, form.field(1)->server_type());
  EXPECT_EQ(ADDRESS_HOME_LINE2, form.field(2)->server_type());

  // Validate that the heuristic prediction wins for the street name and house
  // number.
  EXPECT_THAT(form.field(1)->Type().GetTypes(),
              ElementsAre(ADDRESS_HOME_STREET_NAME));
  EXPECT_THAT(form.field(2)->Type().GetTypes(),
              ElementsAre(ADDRESS_HOME_HOUSE_NUMBER));
}

// Tests that a joined prediction for email or loyalty card fields is generated
// when the server returns separate predictions for each type.
TEST_F(AutofillCrowdsourcingEncoding, ParseQueryResponse_JoinedTypes) {
  base::test::ScopedFeatureList features{
      features::kAutofillEnableEmailOrLoyaltyCardsFilling};
  FormData form_data = test::GetFormData(
      {.fields =
           {// Field accepting the user email of loyalty card.
            {.label = u"Email or Member ID", .name = u"email-or-loyalty-card"},
            // Password field.
            {.label = u"Password",
             .name = u"password",
             .form_control_type = FormControlType::kInputPassword}},
       .url = "http://foo.com"});
  FormStructure form(form_data);
  ParseRationalizeAndSection(form);

  // Setup the query response.
  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  AddFieldPredictionsToForm(form_data.fields()[0],
                            {CreateFieldPrediction(EMAIL_ADDRESS),
                             CreateFieldPrediction(LOYALTY_MEMBERSHIP_ID)},
                            form_suggestion);
  AddFieldPredictionToForm(form_data.fields()[1], PASSWORD, form_suggestion);

  // Parse the response and update the field type predictions.
  std::vector<raw_ptr<FormStructure, VectorExperimental>> forms{&form};
  ParseServerPredictionsQueryResponse(SerializeAndEncode(response), forms,
                                      test::GetEncodedSignatures(forms),
                                      nullptr);
  ASSERT_EQ(form.field_count(), 2U);

  // Validate the heuristic and server predictions.
#if BUILDFLAG(IS_IOS)
  EXPECT_EQ(form.field(0)->heuristic_type(), EMAIL_ADDRESS);
#else
  EXPECT_EQ(form.field(0)->heuristic_type(), EMAIL_OR_LOYALTY_MEMBERSHIP_ID);
#endif
  EXPECT_EQ(form.field(0)->server_type(), EMAIL_OR_LOYALTY_MEMBERSHIP_ID);

  // Validate that the server prediction wins for email or loyalty cards.
  EXPECT_THAT(form.field(0)->Type().GetTypes(),
              ElementsAre(EMAIL_OR_LOYALTY_MEMBERSHIP_ID));
  EXPECT_THAT(form.field(1)->Type().GetTypes(), ElementsAre(PASSWORD));
}

// Tests that a server joined prediction is not generated for email or loyalty
// card fields if the server does not return separate predictions for each type.
TEST_F(AutofillCrowdsourcingEncoding, ParseQueryResponse_NoJoinedTypes) {
  base::test::ScopedFeatureList features{
      features::kAutofillEnableEmailOrLoyaltyCardsFilling};
  FormData form_data = test::GetFormData(
      {.fields =
           {// Field accepting the user email of loyalty card.
            {.label = u"Email or Member ID", .name = u"email-or-loyalty-card"},
            // Password field.
            {.label = u"Password",
             .name = u"password",
             .form_control_type = FormControlType::kInputPassword}},
       .url = "http://foo.com"});
  FormStructure form(form_data);
  ParseRationalizeAndSection(form);

  // Setup the query response.
  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  // Only email predictions are sent.
  AddFieldPredictionsToForm(form_data.fields()[0],
                            {CreateFieldPrediction(EMAIL_ADDRESS)},
                            form_suggestion);
  AddFieldPredictionToForm(form_data.fields()[1], PASSWORD, form_suggestion);

  // Parse the response and update the field type predictions.
  std::vector<raw_ptr<FormStructure, VectorExperimental>> forms{&form};
  ParseServerPredictionsQueryResponse(SerializeAndEncode(response), forms,
                                      test::GetEncodedSignatures(forms),
                                      nullptr);
  ASSERT_EQ(form.field_count(), 2U);

  // Validate the heuristic and server predictions.
#if BUILDFLAG(IS_IOS)
  FieldType heuristic_type = EMAIL_ADDRESS;
#else
  FieldType heuristic_type = EMAIL_OR_LOYALTY_MEMBERSHIP_ID;
#endif
  EXPECT_EQ(form.field(0)->heuristic_type(), heuristic_type);
  EXPECT_EQ(form.field(0)->server_type(), EMAIL_ADDRESS);

  // Validate that the server prediction wins for email or loyalty cards.
  EXPECT_THAT(form.field(0)->Type().GetTypes(), ElementsAre(heuristic_type));
  EXPECT_THAT(form.field(1)->Type().GetTypes(), ElementsAre(PASSWORD));
}

// Tests proper resolution heuristic, server and html field types when the
// server returns NO_SERVER_DATA, UNKNOWN_TYPE, and a valid type.
TEST_F(AutofillCrowdsourcingEncoding, ParseQueryResponse_TooManyTypes) {
  FormData form_data;
  form_data.set_url(GURL("http://foo.com"));
  form_data.set_fields(
      {CreateTestFormField("First Name", "fname", "",
                           FormControlType::kInputText),
       CreateTestFormField("Last Name", "lname", "",
                           FormControlType::kInputText),
       CreateTestFormField("email", "email", "", FormControlType::kInputText,
                           "address-level2")});
  FormStructure form(form_data);
  ParseRationalizeAndSection(form);

  // Setup the query response.
  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  AddFieldPredictionToForm(form_data.fields()[0], NAME_FIRST, form_suggestion);
  AddFieldPredictionToForm(form_data.fields()[1], NAME_LAST, form_suggestion);
  AddFieldPredictionToForm(form_data.fields()[2], ADDRESS_HOME_LINE1,
                           form_suggestion);
  form_suggestion->add_field_suggestions()->add_predictions()->set_type(
      EMAIL_ADDRESS);
  form_suggestion->add_field_suggestions()->add_predictions()->set_type(
      UNKNOWN_TYPE);

  std::string response_string = SerializeAndEncode(response);

  // Parse the response and update the field type predictions.
  std::vector<raw_ptr<FormStructure, VectorExperimental>> forms{&form};
  ParseServerPredictionsQueryResponse(
      response_string, forms, test::GetEncodedSignatures(forms), nullptr);
  ASSERT_EQ(form.field_count(), 3U);

  // Validate field 0.
  EXPECT_EQ(NAME_FIRST, form.field(0)->heuristic_type());
  EXPECT_EQ(NAME_FIRST, form.field(0)->server_type());
  EXPECT_EQ(HtmlFieldType::kUnspecified, form.field(0)->html_type());
  EXPECT_THAT(form.field(0)->Type().GetTypes(), ElementsAre(NAME_FIRST));

  // Validate field 1.
  EXPECT_EQ(NAME_LAST, form.field(1)->heuristic_type());
  EXPECT_EQ(NAME_LAST, form.field(1)->server_type());
  EXPECT_EQ(HtmlFieldType::kUnspecified, form.field(1)->html_type());
  EXPECT_THAT(form.field(1)->Type().GetTypes(), ElementsAre(NAME_LAST));

  // Validate field 2. Note: HtmlFieldType::kAddressLevel2 -> City
  EXPECT_EQ(EMAIL_ADDRESS, form.field(2)->heuristic_type());
  EXPECT_EQ(ADDRESS_HOME_LINE1, form.field(2)->server_type());
  EXPECT_EQ(HtmlFieldType::kAddressLevel2, form.field(2)->html_type());
  EXPECT_THAT(form.field(2)->Type().GetTypes(), ElementsAre(ADDRESS_HOME_CITY));

  // Also check the extreme case of an empty form.
  FormStructure empty_form{FormData()};
  std::vector<raw_ptr<FormStructure, VectorExperimental>> empty_forms{
      &empty_form};
  ParseServerPredictionsQueryResponse(SerializeAndEncode(response), empty_forms,
                                      test::GetEncodedSignatures(empty_forms),
                                      nullptr);
  ASSERT_EQ(empty_form.field_count(), 0U);
}

// Tests proper resolution heuristic, server and html field types when the
// server returns NO_SERVER_DATA, UNKNOWN_TYPE, and a valid type.
TEST_F(AutofillCrowdsourcingEncoding, ParseQueryResponse_UnknownType) {
  FormData form_data;
  form_data.set_url(GURL("http://foo.com"));
  form_data.set_fields(
      {CreateTestFormField("First Name", "fname", "",
                           FormControlType::kInputText),
       CreateTestFormField("Last Name", "lname", "",
                           FormControlType::kInputText),
       CreateTestFormField("email", "email", "", FormControlType::kInputText,
                           "address-level2")});
  FormStructure form(form_data);
  ParseRationalizeAndSection(form);

  // Setup the query response.
  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  AddFieldPredictionToForm(form_data.fields()[0], UNKNOWN_TYPE,
                           form_suggestion);
  AddFieldPredictionToForm(form_data.fields()[1], NO_SERVER_DATA,
                           form_suggestion);
  AddFieldPredictionToForm(form_data.fields()[2], ADDRESS_HOME_LINE1,
                           form_suggestion);

  // Parse the response and update the field type predictions.
  std::vector<raw_ptr<FormStructure, VectorExperimental>> forms{&form};
  ParseServerPredictionsQueryResponse(SerializeAndEncode(response), forms,
                                      test::GetEncodedSignatures(forms),
                                      nullptr);
  ASSERT_EQ(form.field_count(), 3U);

  // Validate field 0.
  EXPECT_EQ(form.field(0)->heuristic_type(), NAME_FIRST);
  EXPECT_EQ(form.field(0)->server_type(), UNKNOWN_TYPE);
  EXPECT_EQ(form.field(0)->html_type(), HtmlFieldType::kUnspecified);
  EXPECT_THAT(form.field(0)->Type().GetTypes(), ElementsAre(UNKNOWN_TYPE));

  // Validate field 1.
  EXPECT_EQ(form.field(1)->heuristic_type(), NAME_LAST);
  EXPECT_EQ(form.field(1)->server_type(), NO_SERVER_DATA);
  EXPECT_EQ(form.field(1)->html_type(), HtmlFieldType::kUnspecified);
  EXPECT_THAT(form.field(1)->Type().GetTypes(), ElementsAre(NAME_LAST));

  // Validate field 2. Note: HtmlFieldType::kAddressLevel2 -> City
  EXPECT_EQ(form.field(2)->heuristic_type(), EMAIL_ADDRESS);
  EXPECT_EQ(form.field(2)->server_type(), ADDRESS_HOME_LINE1);
  EXPECT_EQ(form.field(2)->html_type(), HtmlFieldType::kAddressLevel2);
  EXPECT_THAT(form.field(2)->Type().GetTypes(), ElementsAre(ADDRESS_HOME_CITY));
}

struct PredictionPrecedenceTestCase {
  FieldPrediction main_frame_prediction;
  FieldPrediction iframe_prediction;
  bool autofill_ai_feature_on = false;
  FieldType expected_type;
};

class AutofillCrowdsourcingEncodingPredictionPrecedenceTest
    : public ::testing::TestWithParam<PredictionPrecedenceTestCase> {
 public:
  AutofillCrowdsourcingEncodingPredictionPrecedenceTest() {
    if (GetParam().autofill_ai_feature_on) {
      feature_list_.InitAndEnableFeature(features::kAutofillAiWithDataSchema);
    } else {
      feature_list_.InitAndDisableFeature(features::kAutofillAiWithDataSchema);
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  test::AutofillUnitTestEnvironment autofill_test_environment_;
};

// Tests that precedence of server's query response is indeed: Main frame
// overrides > iframe overrides > main frame crowdsourcing > iframe
// crowdsourcing. Tests that if `kAutofillAiWithDataSchema` is enabled, Autofill
// AI predictions are treated on the same footing as crowdsourcing predictions -
// otherwise, they receive the lowest priority.
TEST_P(AutofillCrowdsourcingEncodingPredictionPrecedenceTest,
       ParseServerPredictionsQueryResponse) {
  constexpr int host_form_signature = 12345;

  // Create an iframe form with a single field.
  std::vector<FormFieldData> fields;
  FormFieldData field;
  field.set_form_control_type(FormControlType::kInputText);
  field.set_name(u"name");
  field.set_renderer_id(test::MakeFieldRendererId());
  field.set_host_form_signature(FormSignature(host_form_signature));
  fields.push_back(field);

  // Create the main frame form.
  FormData form;
  form.set_fields(fields);
  form.set_url(GURL("http://foo.com"));
  FormStructure form_structure(form);
  std::vector<raw_ptr<FormStructure, VectorExperimental>> forms;
  forms.push_back(&form_structure);

  // Make serialized API response.
  AutofillQueryResponse api_response;
  std::vector<FormSignature> encoded_signatures =
      test::GetEncodedSignatures(forms);

  // Main frame response.
  auto* main_frame_form_suggestion = api_response.add_form_suggestions();
  AddFieldPredictionsToForm(field, {GetParam().main_frame_prediction},
                            main_frame_form_suggestion);

  // Iframe response.
  encoded_signatures.emplace_back(host_form_signature);
  auto* iframe_form_suggestion = api_response.add_form_suggestions();
  AddFieldPredictionsToForm(field, {GetParam().iframe_prediction},
                            iframe_form_suggestion);

  // Serialize API response.
  ParseServerPredictionsQueryResponse(SerializeAndEncode(api_response), forms,
                                      encoded_signatures, nullptr);

  ASSERT_EQ(forms.front()->field_count(), 1U);
  EXPECT_EQ(forms.front()->field(0)->server_type(), GetParam().expected_type);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    AutofillCrowdsourcingEncodingPredictionPrecedenceTest,
    ::testing::Values(
        PredictionPrecedenceTestCase{
            .main_frame_prediction = CreateFieldPrediction(EMAIL_ADDRESS,
                                                           false),
            .iframe_prediction = CreateFieldPrediction(NAME_FULL, false),
            .expected_type = EMAIL_ADDRESS},
        PredictionPrecedenceTestCase{
            .main_frame_prediction = CreateFieldPrediction(EMAIL_ADDRESS,
                                                           false),
            .iframe_prediction = CreateFieldPrediction(NAME_FULL, true),
            .expected_type = NAME_FULL},
        PredictionPrecedenceTestCase{
            .main_frame_prediction = CreateFieldPrediction(EMAIL_ADDRESS, true),
            .iframe_prediction = CreateFieldPrediction(NAME_FULL, false),
            .expected_type = EMAIL_ADDRESS},
        PredictionPrecedenceTestCase{
            .main_frame_prediction = CreateFieldPrediction(EMAIL_ADDRESS, true),
            .iframe_prediction = CreateFieldPrediction(NAME_FULL, true),
            .expected_type = EMAIL_ADDRESS},
        PredictionPrecedenceTestCase{
            .main_frame_prediction =
                CreateFieldPrediction(PASSPORT_NUMBER,
                                      FieldPrediction::SOURCE_AUTOFILL_AI),
            .iframe_prediction = CreateFieldPrediction(NAME_FULL, false),
            .expected_type = NAME_FULL},
        PredictionPrecedenceTestCase{
            .main_frame_prediction = CreateFieldPrediction(EMAIL_ADDRESS,
                                                           false),
            .iframe_prediction = CreateFieldPrediction(NAME_FULL, false),
            .autofill_ai_feature_on = true,
            .expected_type = EMAIL_ADDRESS},
        PredictionPrecedenceTestCase{
            .main_frame_prediction = CreateFieldPrediction(EMAIL_ADDRESS,
                                                           false),
            .iframe_prediction = CreateFieldPrediction(NAME_FULL, true),
            .autofill_ai_feature_on = true,
            .expected_type = NAME_FULL},
        PredictionPrecedenceTestCase{
            .main_frame_prediction =
                CreateFieldPrediction(PASSPORT_NUMBER,
                                      FieldPrediction::SOURCE_AUTOFILL_AI),
            .iframe_prediction = CreateFieldPrediction(NAME_FULL, false),
            .autofill_ai_feature_on = true,
            .expected_type = PASSPORT_NUMBER},
        PredictionPrecedenceTestCase{
            .main_frame_prediction = CreateFieldPrediction(
                PASSPORT_NUMBER,
                FieldPrediction::SOURCE_AUTOFILL_AI_CROWDSOURCING),
            .iframe_prediction = CreateFieldPrediction(NAME_FULL, false),
            .autofill_ai_feature_on = true,
            .expected_type = PASSPORT_NUMBER}));

TEST_F(AutofillCrowdsourcingEncoding,
       ParseQueryResponse_MergeAutofillAndPasswordsPredictions) {
  FormData form_data = test::GetFormData(
      {.fields = {
           {.host_form_signature = FormSignature(12345), .name = u"name"}}});

  FormStructure form(form_data);
  ParseRationalizeAndSection(form);

  // Setup the query response.
  AutofillQueryResponse response;
  std::vector<raw_ptr<FormStructure, VectorExperimental>> forms{&form};
  std::vector<FormSignature> encoded_signatures =
      test::GetEncodedSignatures(forms);
  // Main frame response.
  auto* main_frame_form_suggestion = response.add_form_suggestions();
  AddFieldPredictionToForm(form_data.fields()[0], EMAIL_ADDRESS,
                           main_frame_form_suggestion);
  // Iframe response.
  encoded_signatures.emplace_back(12345);
  auto* iframe_form_suggestion = response.add_form_suggestions();
  AddFieldPredictionToForm(form_data.fields()[0], SINGLE_USERNAME,
                           iframe_form_suggestion);

  // Parse the response and update the field type predictions.
  ParseServerPredictionsQueryResponse(SerializeAndEncode(response), forms,
                                      encoded_signatures, nullptr);
  ASSERT_EQ(form.field_count(), 1U);

  // Validate field 0.
  EXPECT_THAT(forms[0]->field(0)->server_predictions(),
              ElementsAre(EqualsPrediction(EMAIL_ADDRESS),
                          EqualsPrediction(SINGLE_USERNAME)));
}

// Tests that the signatures of a field's FormFieldData::host_form_signature are
// used as a fallback if the form's signature does not contain useful type
// predictions.
TEST_F(AutofillCrowdsourcingEncoding,
       ParseServerPredictionsQueryResponse_FallbackToHostFormSignature) {
  // Create a form whose fields have FormFieldData::host_form_signature either
  // 12345 or 67890. The first two fields have identical field signatures.
  FormData form = test::GetFormData(
      {.fields = {
           {.host_form_signature = FormSignature(12345), .name = u"name"},
           {.host_form_signature = FormSignature(12345), .name = u"name"},
           {.host_form_signature = FormSignature(12345), .name = u"number"},
           {.host_form_signature = FormSignature(67890), .name = u"exp_month"},
           {.host_form_signature = FormSignature(67890), .name = u"exp_year"},
           {.host_form_signature = FormSignature(67890), .name = u"cvc"},
           {.host_form_signature = FormSignature(67890)}}});

  std::vector<FieldType> expected_types;
  expected_types.push_back(CREDIT_CARD_NAME_FIRST);
  expected_types.push_back(CREDIT_CARD_NAME_LAST);
  expected_types.push_back(CREDIT_CARD_NUMBER);
  expected_types.push_back(CREDIT_CARD_EXP_MONTH);
  expected_types.push_back(CREDIT_CARD_EXP_2_DIGIT_YEAR);
  expected_types.push_back(CREDIT_CARD_VERIFICATION_CODE);
  expected_types.push_back(NO_SERVER_DATA);

  FormStructure form_structure(form);
  std::vector<raw_ptr<FormStructure, VectorExperimental>> forms;
  forms.push_back(&form_structure);

  ASSERT_GE(form.fields().size(), 6u);

  // Make serialized API response.
  AutofillQueryResponse api_response;
  // Response for the form's signature:
  // - The predictions for `fields[1]`, `fields[2]`, `fields[5]` are expected to
  //   be overridden by the FormFieldData::host_form_signature predictions.
  // - Since fields 0 and 1 have identical signatures, the client must consider
  //   the fields' rank in FormData::host_form_signature's predictions
  //   to obtain the right prediction for `fields[1]`.
  // - `fields[6]` has no predictions at all.
  std::vector<FormSignature> encoded_signatures =
      test::GetEncodedSignatures(forms);
  {
    auto* form_suggestion = api_response.add_form_suggestions();
    AddFieldPredictionToForm(form.fields()[0], expected_types[0],
                             form_suggestion);
    AddFieldPredictionToForm(form.fields()[1], NO_SERVER_DATA, form_suggestion);
    AddFieldPredictionToForm(form.fields()[2], NO_SERVER_DATA, form_suggestion);
    AddFieldPredictionToForm(form.fields()[3], expected_types[3],
                             form_suggestion);
    AddFieldPredictionToForm(form.fields()[4], expected_types[4],
                             form_suggestion);
  }
  // Response for the FormFieldData::host_form_signature 12345.
  encoded_signatures.push_back(FormSignature(12345));
  {
    auto* form_suggestion = api_response.add_form_suggestions();
    AddFieldPredictionToForm(form.fields()[0], NO_SERVER_DATA, form_suggestion);
    AddFieldPredictionToForm(form.fields()[1], expected_types[1],
                             form_suggestion);
    AddFieldPredictionToForm(form.fields()[2], expected_types[2],
                             form_suggestion);
  }
  // Response for the FormFieldData::host_form_signature 67890.
  encoded_signatures.push_back(FormSignature(67890));
  {
    auto* form_suggestion = api_response.add_form_suggestions();
    AddFieldPredictionToForm(form.fields()[4], ADDRESS_HOME_CITY,
                             form_suggestion);
    AddFieldPredictionToForm(form.fields()[5], expected_types[5],
                             form_suggestion);
  }

  ParseServerPredictionsQueryResponse(SerializeAndEncode(api_response), forms,
                                      encoded_signatures, nullptr);

  // Check expected field types.
  ASSERT_GE(forms[0]->field_count(), 6U);
  ASSERT_EQ(forms[0]->field(0)->GetFieldSignature(),
            forms[0]->field(1)->GetFieldSignature());
  EXPECT_EQ(forms.front()->field(0)->server_type(), expected_types[0]);
  EXPECT_EQ(forms.front()->field(1)->server_type(), expected_types[1]);
  EXPECT_EQ(forms.front()->field(2)->server_type(), expected_types[2]);
  EXPECT_EQ(forms.front()->field(3)->server_type(), expected_types[3]);
  EXPECT_EQ(forms.front()->field(4)->server_type(), expected_types[4]);
  EXPECT_EQ(forms.front()->field(5)->server_type(), expected_types[5]);
  EXPECT_EQ(forms.front()->field(6)->server_type(), expected_types[6]);
}

TEST_F(AutofillCrowdsourcingEncoding, ParseServerPredictionsQueryResponse) {
  // Make form 1 data.
  FormData form = test::GetFormData(
      {.fields = {{.label = u"fullname", .name = u"fullname"},
                  {.label = u"address", .name = u"address"},
                  // Checkable fields should be ignored in parsing
                  {.label = u"radio_button",
                   .form_control_type = FormControlType::kInputRadio}}});

  FormStructure form_structure(form);
  std::vector<raw_ptr<FormStructure, VectorExperimental>> forms;
  forms.push_back(&form_structure);

  // Make form 2 data.
  FormData form2 = test::GetFormData(
      {.fields = {
           {.label = u"email", .name = u"email"},
           {.label = u"password",
            .name = u"password",
            .form_control_type = FormControlType::kInputPassword},
       }});

  FormStructure form_structure2(form2);
  forms.push_back(&form_structure2);

  // Make serialized API response.
  AutofillQueryResponse api_response;
  // Make form 1 suggestions.
  auto* form_suggestion = api_response.add_form_suggestions();
  AddFieldPredictionsToForm(form.fields()[0],
                            {CreateFieldPrediction(NAME_FULL),
                             CreateFieldPrediction(CREDIT_CARD_NAME_FULL)},
                            form_suggestion);
  AddFieldPredictionToForm(form.fields()[1], ADDRESS_HOME_LINE1,
                           form_suggestion);
  // Make form 2 suggestions.
  form_suggestion = api_response.add_form_suggestions();
  AddFieldPredictionToForm(form2.fields()[0], EMAIL_ADDRESS, form_suggestion);
  AddFieldPredictionToForm(form2.fields()[1], NO_SERVER_DATA, form_suggestion);
  ParseServerPredictionsQueryResponse(SerializeAndEncode(api_response), forms,
                                      test::GetEncodedSignatures(forms),
                                      nullptr);

  // Verify that the form fields are properly filled with data retrieved from
  // the query.
  ASSERT_GE(forms[0]->field_count(), 2U);
  ASSERT_GE(forms[1]->field_count(), 2U);

  EXPECT_EQ(forms[0]->field(0)->server_type(), NAME_FULL);
  EXPECT_THAT(forms[0]->field(0)->server_predictions(),
              ElementsAre(EqualsPrediction(NAME_FULL),
                          EqualsPrediction(CREDIT_CARD_NAME_FULL)));

  EXPECT_EQ(forms[0]->field(1)->server_type(), ADDRESS_HOME_LINE1);
  EXPECT_THAT(forms[0]->field(1)->server_predictions(),
              ElementsAre(EqualsPrediction(ADDRESS_HOME_LINE1)));

  EXPECT_EQ(forms[1]->field(0)->server_type(), EMAIL_ADDRESS);
  EXPECT_THAT(forms[1]->field(0)->server_predictions(),
              ElementsAre(EqualsPrediction(EMAIL_ADDRESS)));

  EXPECT_EQ(forms[1]->field(1)->server_type(), NO_SERVER_DATA);
  EXPECT_THAT(forms[1]->field(1)->server_predictions(),
              ElementsAre(EqualsPrediction(NO_SERVER_DATA)));
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
// Tests that manually specified (i.e. passed as a feature parameter) field type
// predictions override server predictions.
TEST_F(AutofillCrowdsourcingEncoding,
       ParseServerPredictionsQueryResponseWithManualOverrides) {
  // Make form.
  FormFieldData field1 =
      CreateTestFormField("name", "name", "", FormControlType::kInputText);
  FormFieldData field2 = CreateTestFormField("password", "password", "",
                                             FormControlType::kInputText);
  FormData form;
  form.set_fields({field1, field2});
  form.set_url(GURL("http://foo.com"));
  FormStructure form_structure(form);
  std::vector<raw_ptr<FormStructure, VectorExperimental>> forms{
      &form_structure};

  // The feature is only initialized here because the parameters contain the
  // form and field signatures.
  // Only the prediction for the first field is overridden.
  base::test::ScopedFeatureList features;
  base::FieldTrialParams feature_parameters{
      {features::debug::kAutofillOverridePredictionsSpecification.name,
       CreateManualOverridePrediction({{CalculateFormSignature(form),
                                        CalculateFieldSignatureForField(field1),
                                        {USERNAME}}})}};
  features.InitAndEnableFeatureWithParameters(
      features::debug::kAutofillOverridePredictions, feature_parameters);

  // Make serialized API response.
  AutofillQueryResponse api_response;
  auto* form_suggestion = api_response.add_form_suggestions();
  AddFieldPredictionsToForm(
      form.fields()[0],
      {CreateFieldPrediction(EMAIL_ADDRESS, FieldPrediction::SOURCE_OVERRIDE)},
      form_suggestion);
  AddFieldPredictionsToForm(
      form.fields()[1],
      {CreateFieldPrediction(PASSWORD, FieldPrediction::SOURCE_OVERRIDE)},
      form_suggestion);

  ParseServerPredictionsQueryResponse(SerializeAndEncode(api_response), forms,
                                      test::GetEncodedSignatures(forms),
                                      nullptr);

  ASSERT_EQ(forms[0]->field_count(), 2u);

  // The prediction for the first field comes from the manual override, while
  // the server prediction is used for the second field because no manual
  // override is configured.
  EXPECT_THAT(forms[0]->field(0)->server_predictions(),
              ElementsAre(EqualsPrediction(
                  USERNAME, FieldPrediction::SOURCE_MANUAL_OVERRIDE)));
  EXPECT_THAT(forms[0]->field(1)->server_predictions(),
              ElementsAre(EqualsPrediction(PASSWORD,
                                           FieldPrediction::SOURCE_OVERRIDE)));
}

// Tests that specifying manual field type prediction overrides also works in
// the absence of any server predictions.
TEST_F(
    AutofillCrowdsourcingEncoding,
    ParseServerPredictionsQueryResponseWithManualOverridesAndNoServerPredictions) {
  // Make form.
  FormFieldData field1 =
      CreateTestFormField("name", "name", "", FormControlType::kInputText);
  FormFieldData field2 =
      CreateTestFormField("name", "name", "", FormControlType::kInputText);

  const FieldSignature kFieldSignature =
      CalculateFieldSignatureForField(field1);
  EXPECT_EQ(kFieldSignature, CalculateFieldSignatureForField(field2));

  FormData form;
  form.set_fields({field1, field2});
  form.set_url(GURL("http://foo.com"));
  FormStructure form_structure(form);
  std::vector<raw_ptr<FormStructure, VectorExperimental>> forms{
      &form_structure};
  const FormSignature kFormSignature = CalculateFormSignature(form);

  // The feature is only initialized here because the parameters contain the
  // form and field signatures.
  // Only the prediction for the first field is overridden. The prediction for
  // the following fields with the same signature is defaulted to server
  // predictions, because the last manual type prediction override is a "pass
  // through".
  base::test::ScopedFeatureList features;
  base::FieldTrialParams feature_parameters{
      {features::debug::kAutofillOverridePredictionsSpecification.name,
       CreateManualOverridePrediction(
           {{kFormSignature, kFieldSignature, {NAME_FIRST}},
            {kFormSignature, kFieldSignature, {}}})}};
  features.InitAndEnableFeatureWithParameters(
      features::debug::kAutofillOverridePredictions, feature_parameters);

  // Make serialized API response.
  AutofillQueryResponse api_response;
  ParseServerPredictionsQueryResponse(SerializeAndEncode(api_response), forms,
                                      test::GetEncodedSignatures(forms),
                                      nullptr);

  ASSERT_EQ(forms[0]->field_count(), 2u);

  // The prediction for the first field comes from the manual override. The
  // second one is meant as a pass through for server predictions, but since
  // there are none, there is no prediction.
  EXPECT_THAT(forms[0]->field(0)->server_predictions(),
              ElementsAre(EqualsPrediction(
                  NAME_FIRST, FieldPrediction::SOURCE_MANUAL_OVERRIDE)));
  EXPECT_THAT(forms[0]->field(1)->server_predictions(),
              ElementsAre(EqualsPrediction(
                  NO_SERVER_DATA, FieldPrediction::SOURCE_UNSPECIFIED)));
}

// Tests that (in the case of colliding form and field signatures) specifying a
// pass-through (i.e. no prediction at all) in the last override for that
// form / field signature pair leads to defaulting back to server predictions
// at that position and all other fields with the same form / field signature
// pair that follow.
TEST_F(
    AutofillCrowdsourcingEncoding,
    ParseServerPredictionsQueryResponseWithManualOverridesAndPassthroughInLastPosition) {
  // Make form.
  FormFieldData field1 =
      CreateTestFormField("name", "name", "", FormControlType::kInputText);
  FormFieldData field2 =
      CreateTestFormField("name", "name", "", FormControlType::kInputText);
  FormFieldData field3 =
      CreateTestFormField("name", "name", "", FormControlType::kInputText);

  const FieldSignature kFieldSignature =
      CalculateFieldSignatureForField(field1);
  EXPECT_EQ(kFieldSignature, CalculateFieldSignatureForField(field2));
  EXPECT_EQ(kFieldSignature, CalculateFieldSignatureForField(field3));

  FormData form;
  form.set_fields({field1, field2, field3});
  form.set_url(GURL("http://foo.com"));
  FormStructure form_structure(form);
  std::vector<raw_ptr<FormStructure, VectorExperimental>> forms{
      &form_structure};
  const FormSignature kFormSignature = CalculateFormSignature(form);

  // The feature is only initialized here because the parameters contain the
  // form and field signatures.
  // Only the prediction for the first field is overridden. The prediction for
  // the following fields with the same signature is defaulted to server
  // predictions, because the last manual type prediction override is a "pass
  // through".
  base::test::ScopedFeatureList features;
  base::FieldTrialParams feature_parameters{
      {features::debug::kAutofillOverridePredictionsSpecification.name,
       CreateManualOverridePrediction(
           {{kFormSignature, kFieldSignature, {NAME_FIRST}},
            {kFormSignature, kFieldSignature, {}}})}};
  features.InitAndEnableFeatureWithParameters(
      features::debug::kAutofillOverridePredictions, feature_parameters);

  // Make serialized API response.
  AutofillQueryResponse api_response;
  auto* form_suggestion = api_response.add_form_suggestions();
  AddFieldPredictionsToForm(
      form.fields()[0],
      {CreateFieldPrediction(NAME_FULL, FieldPrediction::SOURCE_OVERRIDE)},
      form_suggestion);
  AddFieldPredictionsToForm(
      form.fields()[1],
      {CreateFieldPrediction(NAME_LAST, FieldPrediction::SOURCE_OVERRIDE)},
      form_suggestion);
  AddFieldPredictionsToForm(
      form.fields()[2],
      {CreateFieldPrediction(COMPANY_NAME, FieldPrediction::SOURCE_OVERRIDE)},
      form_suggestion);

  ParseServerPredictionsQueryResponse(SerializeAndEncode(api_response), forms,
                                      test::GetEncodedSignatures(forms),
                                      nullptr);

  ASSERT_EQ(forms[0]->field_count(), 3u);

  // The prediction for the first field comes from the manual override, while
  // the server prediction is used for the remaining fields.
  EXPECT_THAT(forms[0]->field(0)->server_predictions(),
              ElementsAre(EqualsPrediction(
                  NAME_FIRST, FieldPrediction::SOURCE_MANUAL_OVERRIDE)));
  EXPECT_THAT(forms[0]->field(1)->server_predictions(),
              ElementsAre(EqualsPrediction(NAME_LAST,
                                           FieldPrediction::SOURCE_OVERRIDE)));
  EXPECT_THAT(forms[0]->field(2)->server_predictions(),
              ElementsAre(EqualsPrediction(COMPANY_NAME,
                                           FieldPrediction::SOURCE_OVERRIDE)));
}

// Tests that (in the case of colliding form and field signatures) specifying a
// pass-through (i.e. no prediction at all) in a middle override for that
// form / field signature pair leads to defaulting back to server predictions
// only for that middle field.
TEST_F(
    AutofillCrowdsourcingEncoding,
    ParseServerPredictionsQueryResponseWithManualOverridesAndPassthroughInMiddlePosition) {
  // Make form.
  FormFieldData field1 =
      CreateTestFormField("name", "name", "", FormControlType::kInputText);
  FormFieldData field2 =
      CreateTestFormField("name", "name", "", FormControlType::kInputText);
  FormFieldData field3 =
      CreateTestFormField("name", "name", "", FormControlType::kInputText);
  FormFieldData field4 =
      CreateTestFormField("name", "name", "", FormControlType::kInputText);

  const FieldSignature kFieldSignature =
      CalculateFieldSignatureForField(field1);
  EXPECT_EQ(kFieldSignature, CalculateFieldSignatureForField(field2));
  EXPECT_EQ(kFieldSignature, CalculateFieldSignatureForField(field3));
  EXPECT_EQ(kFieldSignature, CalculateFieldSignatureForField(field4));

  FormData form;
  form.set_fields({field1, field2, field3, field4});
  form.set_url(GURL("http://foo.com"));
  FormStructure form_structure(form);
  std::vector<raw_ptr<FormStructure, VectorExperimental>> forms{
      &form_structure};
  const FormSignature kFormSignature = CalculateFormSignature(form);

  // The feature is only initialized here because the parameters contain the
  // form and field signatures.
  // Only the prediction for the first field is overridden. The prediction for
  // the following fields with the same signature is defaulted to server
  // predictions, because the last manual type prediction override is a "pass
  // through".
  base::test::ScopedFeatureList features;
  base::FieldTrialParams feature_parameters{
      {features::debug::kAutofillOverridePredictionsSpecification.name,
       CreateManualOverridePrediction(
           {{kFormSignature, kFieldSignature, {NAME_FIRST}},
            {kFormSignature, kFieldSignature, {}},
            {kFormSignature, kFieldSignature, {COMPANY_NAME}}})}};
  features.InitAndEnableFeatureWithParameters(
      features::debug::kAutofillOverridePredictions, feature_parameters);

  // Make serialized API response.
  AutofillQueryResponse api_response;
  auto* form_suggestion = api_response.add_form_suggestions();
  AddFieldPredictionsToForm(
      form.fields()[0],
      {CreateFieldPrediction(NAME_LAST, FieldPrediction::SOURCE_OVERRIDE)},
      form_suggestion);

  ParseServerPredictionsQueryResponse(SerializeAndEncode(api_response), forms,
                                      test::GetEncodedSignatures(forms),
                                      nullptr);

  ASSERT_EQ(forms[0]->field_count(), 4u);

  // The prediction for the first field comes from the manual override.
  EXPECT_THAT(forms[0]->field(0)->server_predictions(),
              ElementsAre(EqualsPrediction(
                  NAME_FIRST, FieldPrediction::SOURCE_MANUAL_OVERRIDE)));
  // Since the second manual prediction is a "pass through", the server
  // prediction is used.
  EXPECT_THAT(forms[0]->field(1)->server_predictions(),
              ElementsAre(EqualsPrediction(NAME_LAST,
                                           FieldPrediction::SOURCE_OVERRIDE)));
  // The third (and last) manual override is not a "pass through", so its
  // override is used here.
  EXPECT_THAT(forms[0]->field(2)->server_predictions(),
              ElementsAre(EqualsPrediction(
                  COMPANY_NAME, FieldPrediction::SOURCE_MANUAL_OVERRIDE)));
  // Just as in the case of server predictions, the last prediction is used
  // multiple times if there are more fields than overrides. Since the last
  // manual override was not a "pass through", its value is used.
  EXPECT_THAT(forms[0]->field(3)->server_predictions(),
              ElementsAre(EqualsPrediction(
                  COMPANY_NAME, FieldPrediction::SOURCE_MANUAL_OVERRIDE)));
}

// Tests that manually specified (i.e. passed as a feature parameter)
// alternative_form_signature based field type predictions override
// alternative_form_signature server predictions.
TEST_F(AutofillCrowdsourcingEncoding,
       ParseServerPredictionsQueryResponseOverridesAlternativeFormSignature) {
  // Make form.
  FormFieldData field1 =
      CreateTestFormField("name", "name", "", FormControlType::kInputText);
  FormFieldData field2 = CreateTestFormField("password", "password", "",
                                             FormControlType::kInputText);
  FormData form;
  form.set_fields({field1, field2});
  form.set_url(GURL("http://foo.com"));
  FormStructure form_structure(form);
  std::vector<raw_ptr<FormStructure, VectorExperimental>> forms{
      &form_structure};

  // The feature is only initialized here because the parameters contain the
  // form and field signatures.
  // Only the prediction for the first field is overridden.
  base::test::ScopedFeatureList features;
  base::FieldTrialParams feature_parameters{
      {features::debug::kAutofillOverridePredictionsSpecification.name,
       CreateManualOverridePrediction({{CalculateAlternativeFormSignature(form),
                                        CalculateFieldSignatureForField(field1),
                                        {USERNAME}}})}};
  features.InitAndEnableFeatureWithParameters(
      features::debug::kAutofillOverridePredictions, feature_parameters);

  // Make serialized API response.
  AutofillQueryResponse api_response;
  auto* form_suggestion = api_response.add_form_suggestions();
  AddFieldPredictionsToForm(
      form.fields()[0],
      {CreateFieldPrediction(EMAIL_ADDRESS, FieldPrediction::SOURCE_OVERRIDE)},
      form_suggestion);
  AddFieldPredictionsToForm(
      form.fields()[1],
      {CreateFieldPrediction(PASSWORD, FieldPrediction::SOURCE_OVERRIDE)},
      form_suggestion);

  ParseServerPredictionsQueryResponse(
      SerializeAndEncode(api_response), forms,
      test::GetEncodedAlternativeSignatures(forms), nullptr);

  ASSERT_EQ(forms[0]->field_count(), 2u);

  // The prediction for the first field comes from the manual override, while
  // the server prediction is used for the second field because no manual
  // override is configured.
  EXPECT_THAT(forms[0]->field(0)->server_predictions(),
              ElementsAre(EqualsPrediction(
                  USERNAME, FieldPrediction::SOURCE_MANUAL_OVERRIDE)));
  EXPECT_THAT(forms[0]->field(1)->server_predictions(),
              ElementsAre(EqualsPrediction(PASSWORD,
                                           FieldPrediction::SOURCE_OVERRIDE)));
}

// Tests that manually specified (i.e. passed as a feature parameter)
// alternative_form_signature based field type predictions override
// form_signature server predictions.
TEST_F(
    AutofillCrowdsourcingEncoding,
    ParseServerPredictionsQueryResponseServerOverridesAlternativeFormSignature) {
  // Make form.
  FormFieldData field1 =
      CreateTestFormField("name", "name", "", FormControlType::kInputText);
  FormFieldData field2 = CreateTestFormField("password", "password", "",
                                             FormControlType::kInputText);
  FormData form;
  form.set_fields({field1, field2});
  form.set_url(GURL("http://foo.com"));
  FormStructure form_structure(form);
  std::vector<raw_ptr<FormStructure, VectorExperimental>> forms{
      &form_structure};

  // The feature is only initialized here because the parameters contain the
  // form and field signatures.
  // Only the prediction for the first field is overridden.
  base::test::ScopedFeatureList features;
  base::FieldTrialParams feature_parameters{
      {features::debug::kAutofillOverridePredictionsSpecification.name,
       CreateManualOverridePrediction({{CalculateAlternativeFormSignature(form),
                                        CalculateFieldSignatureForField(field1),
                                        {USERNAME}}})}};
  features.InitAndEnableFeatureWithParameters(
      features::debug::kAutofillOverridePredictions, feature_parameters);

  // Make serialized API response.
  AutofillQueryResponse api_response;
  auto* form_suggestion = api_response.add_form_suggestions();
  AddFieldPredictionsToForm(
      form.fields()[0],
      {CreateFieldPrediction(EMAIL_ADDRESS,
                             FieldPrediction::SOURCE_PASSWORDS_DEFAULT)},
      form_suggestion);
  AddFieldPredictionsToForm(
      form.fields()[1],
      {CreateFieldPrediction(PASSWORD,
                             FieldPrediction::SOURCE_PASSWORDS_DEFAULT)},
      form_suggestion);

  ParseServerPredictionsQueryResponse(SerializeAndEncode(api_response), forms,
                                      test::GetEncodedSignatures(forms),
                                      nullptr);

  ASSERT_EQ(forms[0]->field_count(), 2u);

  // The prediction for the first field comes from the server override.
  EXPECT_THAT(forms[0]->field(0)->server_predictions(),
              ElementsAre(EqualsPrediction(
                  USERNAME, FieldPrediction::SOURCE_MANUAL_OVERRIDE)));
  EXPECT_THAT(forms[0]->field(1)->server_predictions(),
              ElementsAre(EqualsPrediction(
                  PASSWORD, FieldPrediction::SOURCE_PASSWORDS_DEFAULT)));
}

// Tests that server overrides have lower priority than manual overrides.
TEST_F(
    AutofillCrowdsourcingEncoding,
    ParseServerPredictionsQueryResponseReplaceServerOverrideWithManualOverride) {
  FormFieldData name_field =
      CreateTestFormField("name", "name", "", FormControlType::kInputText);
  FormFieldData password_field = CreateTestFormField(
      "password", "password", "", FormControlType::kInputText);
  FormData form;
  form.set_fields({name_field, password_field});
  form.set_url(GURL("http://foo.com"));
  FormStructure form_structure(form);
  std::vector<raw_ptr<FormStructure, VectorExperimental>> forms{
      &form_structure};

  // The feature is only initialized here because the parameters contain the
  // form and field signatures. Only the prediction for the first field is
  // overridden.
  base::test::ScopedFeatureList features;
  base::FieldTrialParams feature_parameters{
      {features::debug::kAutofillOverridePredictionsSpecification.name,
       CreateManualOverridePrediction(
           {{CalculateAlternativeFormSignature(form),
             CalculateFieldSignatureForField(name_field),
             {USERNAME}}})}};
  features.InitAndEnableFeatureWithParameters(
      features::debug::kAutofillOverridePredictions, feature_parameters);

  // Make serialized API response.
  AutofillQueryResponse api_response;
  auto* form_suggestion = api_response.add_form_suggestions();
  AddFieldPredictionsToForm(
      form.fields()[0],
      {CreateFieldPrediction(EMAIL_ADDRESS, FieldPrediction::SOURCE_OVERRIDE)},
      form_suggestion);
  AddFieldPredictionsToForm(
      form.fields()[1],
      {CreateFieldPrediction(PASSWORD, FieldPrediction::SOURCE_OVERRIDE)},
      form_suggestion);

  ParseServerPredictionsQueryResponse(SerializeAndEncode(api_response), forms,
                                      test::GetEncodedSignatures(forms),
                                      nullptr);

  ASSERT_EQ(forms[0]->field_count(), 2u);

  // The prediction for the first field comes from the manual override.
  EXPECT_THAT(forms[0]->field(0)->server_predictions(),
              ElementsAre(EqualsPrediction(
                  USERNAME, FieldPrediction::SOURCE_MANUAL_OVERRIDE)));
  EXPECT_THAT(forms[0]->field(1)->server_predictions(),
              ElementsAre(EqualsPrediction(PASSWORD,
                                           FieldPrediction::SOURCE_OVERRIDE)));
}
#endif

// Tests ParseServerPredictionsQueryResponse when the payload cannot be parsed
// to an AutofillQueryResponse where we expect an early return of the function.
TEST_F(AutofillCrowdsourcingEncoding,
       ParseServerPredictionsQueryResponseWhenCannotParseProtoFromString) {
  FormData form;
  form.set_url(GURL("http://foo.com"));
  form.set_fields({CreateTestFormField("emailaddress", "emailaddress", "",
                                       FormControlType::kInputEmail)});

  // Add form to the vector needed by the response parsing function.
  FormStructure form_structure(form);
  form_structure.field(0)->set_server_predictions(
      {CreateFieldPrediction(NAME_FULL)});
  std::vector<raw_ptr<FormStructure, VectorExperimental>> forms;
  forms.push_back(&form_structure);

  std::string response_string = "invalid string that cannot be parsed";
  ParseServerPredictionsQueryResponse(std::move(response_string), forms,
                                      test::GetEncodedSignatures(forms),
                                      nullptr);

  // Verify that the form fields remain intact because
  // ParseServerPredictionsQueryResponse could not parse the server's response
  // because it was badly serialized.
  ASSERT_GE(forms[0]->field_count(), 1U);
  EXPECT_EQ(NAME_FULL, forms[0]->field(0)->server_type());
}

// Tests ParseServerPredictionsQueryResponse when the payload is not base64
// where we expect an early return of the function.
TEST_F(AutofillCrowdsourcingEncoding,
       ParseServerPredictionsQueryResponseWhenPayloadNotBase64) {
  FormData form;
  form.set_url(GURL("http://foo.com"));
  form.set_fields({CreateTestFormField("emailaddress", "emailaddress", "",
                                       FormControlType::kInputEmail)});

  // Add form to the vector needed by the response parsing function.
  FormStructure form_structure(form);
  form_structure.field(0)->set_server_predictions(
      {CreateFieldPrediction(NAME_FULL)});
  std::vector<raw_ptr<FormStructure, VectorExperimental>> forms;
  forms.push_back(&form_structure);

  // Make a really simple serialized API response. We don't encode it in base64.
  AutofillQueryResponse api_response;
  auto* form_suggestion = api_response.add_form_suggestions();
  // Here the server gives EMAIL_ADDRESS for field of the form, which should
  // override NAME_FULL that we originally put in the form field if there
  // is no issue when parsing the query response. In this test case there is an
  // issue with the encoding of the data, hence EMAIL_ADDRESS should not be
  // applied because of early exit of the parsing function.
  AddFieldPredictionToForm(form.fields()[0], EMAIL_ADDRESS, form_suggestion);

  // Serialize API response.
  std::string response_string;
  ASSERT_TRUE(api_response.SerializeToString(&response_string));

  ParseServerPredictionsQueryResponse(
      response_string, forms, test::GetEncodedSignatures(forms), nullptr);

  // Verify that the form fields remain intact because
  // ParseServerPredictionsQueryResponse could not parse the server's response
  // that was badly encoded.
  ASSERT_GE(forms[0]->field_count(), 1U);
  EXPECT_EQ(NAME_FULL, forms[0]->field(0)->server_type());
}

TEST_F(AutofillCrowdsourcingEncoding, ParseQueryResponse_AuthorDefinedTypes) {
  FormData form;
  form.set_url(GURL("http://foo.com"));
  form.set_fields(
      {CreateTestFormField("email", "email", "", FormControlType::kInputText,
                           "email"),
       CreateTestFormField("password", "password", "",
                           FormControlType::kInputPassword, "new-password")});
  FormStructure form_structure(form);
  ParseRationalizeAndSection(form_structure);

  std::vector<raw_ptr<FormStructure, VectorExperimental>> forms = {
      &form_structure};

  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  AddFieldPredictionToForm(form.fields()[0], EMAIL_ADDRESS, form_suggestion);
  AddFieldPredictionToForm(form.fields()[1], ACCOUNT_CREATION_PASSWORD,
                           form_suggestion);

  ParseServerPredictionsQueryResponse(SerializeAndEncode(response), forms,
                                      test::GetEncodedSignatures(forms),
                                      nullptr);

  ASSERT_GE(forms[0]->field_count(), 2U);
  // Server type is parsed from the response and is the end result type.
  EXPECT_EQ(forms[0]->field(0)->server_type(), EMAIL_ADDRESS);
  EXPECT_THAT(forms[0]->field(0)->Type().GetTypes(),
              ElementsAre(EMAIL_ADDRESS));
  EXPECT_EQ(forms[0]->field(1)->server_type(), ACCOUNT_CREATION_PASSWORD);
  EXPECT_THAT(forms[0]->field(1)->Type().GetTypes(),
              ElementsAre(ACCOUNT_CREATION_PASSWORD));
}

// Tests that, when the flag is off, we will not set the predicted type to
// unknown for fields that have no server data and autocomplete off, and when
// the flag is ON, we will overwrite the predicted type.
TEST_F(AutofillCrowdsourcingEncoding,
       NoServerData_AutocompleteOff_FlagDisabled_NoOverwrite) {
  FormData form = test::GetFormData(
      {.fields = {// Autocomplete Off, with server data.
                  {.label = u"First Name",
                   .name = u"firstName",
                   .should_autocomplete = false},
                  // Autocomplete Off, without server data.
                  {.label = u"Last Name",
                   .name = u"lastName",
                   .should_autocomplete = false},
                  // Autocomplete On, with server data.
                  {.label = u"Address", .name = u"address"},
                  // Autocomplete On, without server data.
                  {.label = u"Country", .name = u"country"}}});

  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  AddFieldPredictionToForm(form.fields()[0], NAME_FIRST, form_suggestion);
  AddFieldPredictionToForm(form.fields()[1], NO_SERVER_DATA, form_suggestion);
  AddFieldPredictionToForm(form.fields()[2], NO_SERVER_DATA, form_suggestion);
  AddFieldPredictionToForm(form.fields()[3], NO_SERVER_DATA, form_suggestion);

  FormStructure form_structure(form);
  ParseRationalizeAndSection(form_structure);

  // Will call RationalizeFieldTypePredictions
  std::vector<raw_ptr<FormStructure, VectorExperimental>> forms = {
      &form_structure};
  ParseServerPredictionsQueryResponse(SerializeAndEncode(response), forms,
                                      test::GetEncodedSignatures(forms),
                                      nullptr);

  ASSERT_EQ(1U, forms.size());
  ASSERT_EQ(4U, forms[0]->field_count());

  // Only NAME_LAST should be affected by the flag.
  EXPECT_THAT(forms[0]->field(1)->Type().GetTypes(), ElementsAre(NAME_LAST));

  EXPECT_THAT(forms[0]->field(0)->Type().GetTypes(), ElementsAre(NAME_FIRST));
  EXPECT_THAT(forms[0]->field(2)->Type().GetTypes(),
              ElementsAre(ADDRESS_HOME_LINE1));
  EXPECT_THAT(forms[0]->field(3)->Type().GetTypes(),
              ElementsAre(ADDRESS_HOME_COUNTRY));
}

// Tests that we never overwrite the CVC heuristic-predicted type, even if there
// is no server data (votes) for every CC fields.
TEST_F(AutofillCrowdsourcingEncoding, NoServerDataCCFields_CVC_NoOverwrite) {
  // All fields with autocomplete off and no server data.
  FormData form = test::GetFormData(
      {.fields = {
           {.label = u"Cardholder Name",
            .name = u"fullName",
            .should_autocomplete = false},
           {.label = u"Credit Card Number",
            .name = u"cc-number",
            .should_autocomplete = false},
           {.label = u"Expiration Date",
            .name = u"exp-date",
            .should_autocomplete = false},
           {.label = u"CVC", .name = u"cvc", .should_autocomplete = false}}});

  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  AddFieldPredictionToForm(form.fields()[0], NO_SERVER_DATA, form_suggestion);
  AddFieldPredictionToForm(form.fields()[1], NO_SERVER_DATA, form_suggestion);
  AddFieldPredictionToForm(form.fields()[2], NO_SERVER_DATA, form_suggestion);
  AddFieldPredictionToForm(form.fields()[3], NO_SERVER_DATA, form_suggestion);

  FormStructure form_structure(form);
  ParseRationalizeAndSection(form_structure);

  // Will call RationalizeFieldTypePredictions
  std::vector<raw_ptr<FormStructure, VectorExperimental>> forms = {
      &form_structure};
  ParseServerPredictionsQueryResponse(SerializeAndEncode(response), forms,
                                      test::GetEncodedSignatures(forms),
                                      nullptr);

  ASSERT_EQ(1U, forms.size());
  ASSERT_EQ(4U, forms[0]->field_count());
  EXPECT_THAT(forms[0]->field(0)->Type().GetTypes(),
              ElementsAre(CREDIT_CARD_NAME_FULL));
  EXPECT_THAT(forms[0]->field(1)->Type().GetTypes(),
              ElementsAre(CREDIT_CARD_NUMBER));
  EXPECT_THAT(forms[0]->field(2)->Type().GetTypes(),
              ElementsAre(CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR));

  // Regardless of the flag, the CVC field should not have been overwritten.
  EXPECT_THAT(forms[0]->field(3)->Type().GetTypes(),
              ElementsAre(CREDIT_CARD_VERIFICATION_CODE));
}

// Tests that we never overwrite the CVC heuristic-predicted type, even if there
// is server data (votes) for every other CC fields.
TEST_F(AutofillCrowdsourcingEncoding, WithServerDataCCFields_CVC_NoOverwrite) {
  // All fields with autocomplete off and no server data.
  FormData form = test::GetFormData(
      {.fields = {
           {.label = u"Cardholder Name",
            .name = u"fullName",
            .should_autocomplete = false},
           {.label = u"Credit Card Number",
            .name = u"cc-number",
            .should_autocomplete = false},
           {.label = u"Expiration Date",
            .name = u"exp-date",
            .should_autocomplete = false},
           {.label = u"CVC", .name = u"cvc", .should_autocomplete = false}}});

  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  AddFieldPredictionToForm(form.fields()[0], CREDIT_CARD_NAME_FULL,
                           form_suggestion);
  AddFieldPredictionToForm(form.fields()[1], CREDIT_CARD_NUMBER,
                           form_suggestion);
  AddFieldPredictionToForm(form.fields()[2], CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR,
                           form_suggestion);
  AddFieldPredictionToForm(form.fields()[3], NO_SERVER_DATA, form_suggestion);

  FormStructure form_structure(form);
  ParseRationalizeAndSection(form_structure);

  // Will call RationalizeFieldTypePredictions
  std::vector<raw_ptr<FormStructure, VectorExperimental>> forms = {
      &form_structure};
  ParseServerPredictionsQueryResponse(SerializeAndEncode(response), forms,
                                      test::GetEncodedSignatures(forms),
                                      nullptr);

  ASSERT_EQ(1U, forms.size());
  ASSERT_EQ(4U, forms[0]->field_count());

  // Regardless of the flag, the fields should not have been overwritten,
  // including the CVC field.
  EXPECT_THAT(forms[0]->field(0)->Type().GetTypes(),
              ElementsAre(CREDIT_CARD_NAME_FULL));
  EXPECT_THAT(forms[0]->field(1)->Type().GetTypes(),
              ElementsAre(CREDIT_CARD_NUMBER));
  EXPECT_THAT(forms[0]->field(2)->Type().GetTypes(),
              ElementsAre(CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR));
  EXPECT_THAT(forms[0]->field(3)->Type().GetTypes(),
              ElementsAre(CREDIT_CARD_VERIFICATION_CODE));
}

// When two fields have the same signature and the server response has multiple
// predictions for that signature, apply the server predictions in the order
// that they were received.
TEST_F(AutofillCrowdsourcingEncoding, ParseQueryResponse_RankEqualSignatures) {
  FormData form_data;
  FormFieldData field;
  form_data.set_url(GURL("http://foo.com"));
  form_data.set_fields(
      {CreateTestFormField("First Name", "name", "",
                           FormControlType::kInputText),
       CreateTestFormField("Last Name", "name", "",
                           FormControlType::kInputText),
       CreateTestFormField("email", "email", "", FormControlType::kInputText,
                           "address-level2")});

  ASSERT_EQ(CalculateFieldSignatureForField(form_data.fields()[0]),
            CalculateFieldSignatureForField(form_data.fields()[1]));

  FormStructure form(form_data);
  ParseRationalizeAndSection(form);

  // Setup the query response.
  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  AddFieldPredictionToForm(form_data.fields()[0], NAME_FIRST, form_suggestion);
  AddFieldPredictionToForm(form_data.fields()[1], NAME_LAST, form_suggestion);
  AddFieldPredictionToForm(form_data.fields()[2], EMAIL_ADDRESS,
                           form_suggestion);

  // Parse the response and update the field type predictions.
  std::vector<raw_ptr<FormStructure, VectorExperimental>> forms{&form};
  ParseServerPredictionsQueryResponse(SerializeAndEncode(response), forms,
                                      test::GetEncodedSignatures(forms),
                                      nullptr);
  ASSERT_EQ(form.field_count(), 3U);

  EXPECT_EQ(form.field(0)->server_type(), NAME_FIRST);
  EXPECT_EQ(form.field(1)->server_type(), NAME_LAST);
  EXPECT_EQ(form.field(2)->server_type(), EMAIL_ADDRESS);
}

// When two fields have the same signature and the server response has one
// prediction, apply the prediction to every field with that signature.
TEST_F(AutofillCrowdsourcingEncoding,
       ParseQueryResponse_EqualSignaturesFewerPredictions) {
  FormData form_data;
  form_data.set_url(GURL("http://foo.com"));
  form_data.set_fields(
      {CreateTestFormField("First Name", "name", "",
                           FormControlType::kInputText),
       CreateTestFormField("Last Name", "name", "",
                           FormControlType::kInputText),
       CreateTestFormField("email", "email", "", FormControlType::kInputText,
                           "address-level2")});

  ASSERT_EQ(CalculateFieldSignatureForField(form_data.fields()[0]),
            CalculateFieldSignatureForField(form_data.fields()[1]));

  FormStructure form(form_data);
  ParseRationalizeAndSection(form);

  // Setup the query response.
  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  AddFieldPredictionToForm(form_data.fields()[0], NAME_FIRST, form_suggestion);
  AddFieldPredictionToForm(form_data.fields()[2], EMAIL_ADDRESS,
                           form_suggestion);

  // Parse the response and update the field type predictions.
  std::vector<raw_ptr<FormStructure, VectorExperimental>> forms{&form};
  ParseServerPredictionsQueryResponse(SerializeAndEncode(response), forms,
                                      test::GetEncodedSignatures(forms),
                                      nullptr);
  ASSERT_EQ(form.field_count(), 3U);

  EXPECT_EQ(form.field(0)->server_type(), NAME_FIRST);
  // This field gets the same signature as the previous one, because they have
  // the same signature.
  EXPECT_EQ(form.field(1)->server_type(), NAME_FIRST);
  EXPECT_EQ(form.field(2)->server_type(), EMAIL_ADDRESS);
}

// Tests that the `run_autofill_ai_model` of the `AutofillQueryResponse` proto
// is parsed properly.
TEST_F(AutofillCrowdsourcingEncoding, ParseRunAutofillAiModel) {
  // All fields with autocomplete off and no server data.
  FormData form = test::GetFormData({.fields = {{.label = u"First Name"}}});

  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  form_suggestion->set_run_autofill_ai_model(true);

  FormStructure form_structure(form);
  EXPECT_FALSE(form_structure.may_run_autofill_ai_model());
  ParseServerPredictionsQueryResponse(
      SerializeAndEncode(response), {&form_structure},
      test::GetEncodedSignatures({&form_structure}), nullptr);
  EXPECT_TRUE(form_structure.may_run_autofill_ai_model());
}

// Tests that valid format strings are accepted and invalid ones are ignored.
TEST_F(AutofillCrowdsourcingEncoding, ParseFormatString) {
  base::test::ScopedFeatureList features{features::kAutofillAiWithDataSchema};
  FormData form_data;
  form_data.set_url(GURL("https://foo.com"));
  form_data.set_fields(
      {CreateTestFormField("Name", "name", "", FormControlType::kInputText),
       CreateTestFormField("Driver's license number", "dl_number", "",
                           FormControlType::kInputText),
       CreateTestFormField("Passport number", "passport_number", "",
                           FormControlType::kInputText, "")});

  FormStructure form(form_data);
  ParseRationalizeAndSection(form);

  auto add_autofill_ai_prediction =
      [](const FormFieldData& field, FieldType field_type,
         FormatString_Type format_string_type,
         std::string_view format_string_value,
         AutofillQueryResponse_FormSuggestion* form_suggestion) {
        AutofillQueryResponse_FormSuggestion_FieldSuggestion* field_suggestion =
            form_suggestion->add_field_suggestions();
        field_suggestion->set_field_signature(
            *CalculateFieldSignatureForField(field));
        AutofillQueryResponse_FormSuggestion_FieldSuggestion_FieldPrediction*
            prediction = field_suggestion->add_predictions();
        prediction->set_type(field_type);
        prediction->set_source(FieldPrediction::SOURCE_AUTOFILL_AI);
        field_suggestion->mutable_format_string()->set_format_string(
            format_string_value);
        field_suggestion->mutable_format_string()->set_type(format_string_type);
      };

  // Setup the query response.
  AutofillQueryResponse response;
  auto* form_suggestion = response.add_form_suggestions();
  add_autofill_ai_prediction(form_data.fields()[1], DRIVERS_LICENSE_NUMBER,
                             FormatString_Type_AFFIX, "-4", form_suggestion);
  add_autofill_ai_prediction(form_data.fields()[2], PASSPORT_NUMBER,
                             FormatString_Type_AFFIX, "asd", form_suggestion);

  // Parse the response.
  std::vector<raw_ptr<FormStructure, VectorExperimental>> forms{&form};
  ParseServerPredictionsQueryResponse(SerializeAndEncode(response), forms,
                                      test::GetEncodedSignatures(forms),
                                      nullptr);
  ASSERT_EQ(form.field_count(), 3U);

  EXPECT_THAT(form.field(1)->Type().GetTypes(),
              ElementsAre(DRIVERS_LICENSE_NUMBER));
  EXPECT_THAT(form.field(1)->format_string(),
              Optional(AutofillFormatString(u"-4", FormatString_Type_AFFIX)));
  EXPECT_THAT(form.field(2)->Type().GetTypes(), ElementsAre(PASSPORT_NUMBER));
  EXPECT_EQ(form.field(2)->format_string(), std::nullopt);
}

}  // namespace
}  // namespace autofill
