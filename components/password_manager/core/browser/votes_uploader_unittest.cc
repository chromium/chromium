// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/votes_uploader.h"

#include <optional>
#include <string>
#include <utility>

#include "base/hash/hash.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/crowdsourcing/mock_autofill_crowdsourcing_manager.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/test_utils/vote_uploads_test_matchers.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_store/mock_password_store_interface.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/vote_uploads_test_matchers.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

using ::autofill::AutofillCrowdsourcingManager;
using ::autofill::CONFIRMATION_PASSWORD;
using ::autofill::FieldRendererId;
using ::autofill::FieldSignature;
using ::autofill::FormData;
using ::autofill::FormFieldData;
using ::autofill::FormSignature;
using ::autofill::FormStructure;
using ::autofill::NEW_PASSWORD;
using ::autofill::NOT_USERNAME;
using ::autofill::PASSWORD;
using ::autofill::PasswordAttribute;
using ::autofill::ServerFieldType;
using ::autofill::ServerFieldTypeSet;
using ::autofill::SignatureIsSameAs;
using ::autofill::SINGLE_USERNAME;
using ::autofill::SubmissionEventIsSameAs;
using ::autofill::UNKNOWN_TYPE;
using ::autofill::mojom::SubmissionIndicatorEvent;
using ::base::ASCIIToUTF16;
using ::testing::_;
using ::testing::AllOf;
using ::testing::AnyNumber;
using ::testing::IsNull;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SaveArg;

constexpr int kNumberOfPasswordAttributes =
    static_cast<int>(PasswordAttribute::kPasswordAttributesCount);

constexpr FieldRendererId kSingleUsernameRendererId(101);
constexpr FieldSignature kSingleUsernameFieldSignature(1234);
constexpr FormSignature kSingleUsernameFormSignature(1000);

FormPredictions MakeSimpleSingleUsernamePredictions() {
  FormPredictions form_predictions;
  form_predictions.form_signature = kSingleUsernameFormSignature;
  form_predictions.fields.emplace_back();
  form_predictions.fields.back().renderer_id = kSingleUsernameRendererId;
  form_predictions.fields.back().signature = kSingleUsernameFieldSignature;
  return form_predictions;
}

autofill::AutofillUploadContents::SingleUsernameData
MakeSimpleSingleUsernameData() {
  autofill::AutofillUploadContents::SingleUsernameData single_username_data;
  single_username_data.set_username_form_signature(
      kSingleUsernameFormSignature.value());
  single_username_data.set_username_field_signature(
      kSingleUsernameFieldSignature.value());
  single_username_data.set_value_type(
      autofill::AutofillUploadContents::USERNAME_LIKE);
  return single_username_data;
}

class MockPasswordManagerClient : public StubPasswordManagerClient {
 public:
  MOCK_METHOD0(GetAutofillCrowdsourcingManager,
               AutofillCrowdsourcingManager*());
};

}  // namespace

class VotesUploaderTest : public testing::Test {
 public:
  VotesUploaderTest() {
    EXPECT_CALL(client_, GetAutofillCrowdsourcingManager())
        .WillRepeatedly(Return(&mock_autofill_crowdsourcing_manager_));

    ON_CALL(mock_autofill_crowdsourcing_manager_, StartUploadRequest)
        .WillByDefault(Return(true));

    // Create |fields| in |form_to_upload_| and |submitted_form_|. Only |name|
    // field in FormFieldData is important. Set them to the unique values based
    // on index.
    const size_t kNumberOfFields = 20;
    for (size_t i = 0; i < kNumberOfFields; ++i) {
      FormFieldData field;
      field.name = GetFieldNameByIndex(i);
      field.unique_renderer_id = FieldRendererId(i);
      form_to_upload_.form_data.fields.push_back(field);
      submitted_form_.form_data.fields.push_back(field);
    }
    // Password attributes uploading requires a non-empty password value.
    form_to_upload_.password_value = u"password_value";
    submitted_form_.password_value = u"password_value";
  }

 protected:
  std::u16string GetFieldNameByIndex(size_t index) {
    return u"field" + base::NumberToString16(index);
  }

  base::test::TaskEnvironment task_environment_;
  NiceMock<autofill::MockAutofillCrowdsourcingManager>
      mock_autofill_crowdsourcing_manager_{/*client=*/nullptr};
  MockPasswordManagerClient client_;

  PasswordForm form_to_upload_;
  PasswordForm submitted_form_;

  std::string login_form_signature_ = "123";
};

TEST_F(VotesUploaderTest, UploadPasswordVoteUpdate) {
  VotesUploader votes_uploader(&client_, true);
  std::u16string new_password_element = GetFieldNameByIndex(3);
  std::u16string confirmation_element = GetFieldNameByIndex(11);
  form_to_upload_.new_password_element_renderer_id = FieldRendererId(3);
  submitted_form_.new_password_element_renderer_id = FieldRendererId(3);
  form_to_upload_.confirmation_password_element_renderer_id =
      FieldRendererId(11);
  submitted_form_.confirmation_password_element_renderer_id =
      FieldRendererId(11);
  form_to_upload_.new_password_value = u"new_password_value";
  submitted_form_.new_password_value = u"new_password_value";
  submitted_form_.submission_event =
      SubmissionIndicatorEvent::HTML_FORM_SUBMISSION;
  ServerFieldTypeSet expected_field_types = {NEW_PASSWORD,
                                             CONFIRMATION_PASSWORD};
  std::map<std::u16string, ServerFieldType> expected_types = {
      {new_password_element, NEW_PASSWORD},
      {confirmation_element, CONFIRMATION_PASSWORD}};
  SubmissionIndicatorEvent expected_submission_event =
      SubmissionIndicatorEvent::HTML_FORM_SUBMISSION;

  EXPECT_CALL(mock_autofill_crowdsourcing_manager_,
              StartUploadRequest(
                  AllOf(SignatureIsSameAs(form_to_upload_),
                        UploadedAutofillTypesAre(expected_types),
                        SubmissionEventIsSameAs(expected_submission_event)),
                  false, expected_field_types, login_form_signature_, true,
                  nullptr, /*observer=*/IsNull()));

  EXPECT_TRUE(votes_uploader.UploadPasswordVote(
      form_to_upload_, submitted_form_, NEW_PASSWORD, login_form_signature_));
}

TEST_F(VotesUploaderTest, UploadPasswordVoteSave) {
  VotesUploader votes_uploader(&client_, false);
  form_to_upload_.password_element_renderer_id = FieldRendererId(5);
  submitted_form_.password_element_renderer_id = FieldRendererId(5);
  form_to_upload_.confirmation_password_element_renderer_id =
      FieldRendererId(12);
  submitted_form_.confirmation_password_element_renderer_id =
      FieldRendererId(12);
  submitted_form_.submission_event =
      SubmissionIndicatorEvent::HTML_FORM_SUBMISSION;
  ServerFieldTypeSet expected_field_types = {PASSWORD, CONFIRMATION_PASSWORD};
  SubmissionIndicatorEvent expected_submission_event =
      SubmissionIndicatorEvent::HTML_FORM_SUBMISSION;

  EXPECT_CALL(mock_autofill_crowdsourcing_manager_,
              StartUploadRequest(
                  SubmissionEventIsSameAs(expected_submission_event), false,
                  expected_field_types, login_form_signature_, true,
                  /* pref_service= */ nullptr, /*observer=*/IsNull()));

  EXPECT_TRUE(votes_uploader.UploadPasswordVote(
      form_to_upload_, submitted_form_, PASSWORD, login_form_signature_));
}

// Checks votes uploading when
// 1. User saves a credential on a sign-up, but Chrome picked the wrong
// field as username.
// 2. The user modifies the username on login form before submission.
TEST_F(VotesUploaderTest, UploadUsernameOverwrittenVote) {
  VotesUploader votes_uploader(&client_, false);

  form_to_upload_.username_element_renderer_id = FieldRendererId(6);
  form_to_upload_.password_element_renderer_id = FieldRendererId(5);

  std::map<std::u16string, ServerFieldType> expected_types = {
      {GetFieldNameByIndex(6), autofill::USERNAME},
      {GetFieldNameByIndex(5), autofill::ACCOUNT_CREATION_PASSWORD}};
  ServerFieldTypeSet expected_field_types = {
      autofill::ACCOUNT_CREATION_PASSWORD, autofill::USERNAME};

  EXPECT_CALL(
      mock_autofill_crowdsourcing_manager_,
      StartUploadRequest(
          AllOf(UploadedAutofillTypesAre(expected_types),
                UsernameVoteTypeIsSameAs(autofill::AutofillUploadContents::
                                             Field::USERNAME_OVERWRITTEN)),
          false, expected_field_types, login_form_signature_, true, nullptr,
          /*observer=*/IsNull()));

  EXPECT_TRUE(votes_uploader.UploadPasswordVote(
      form_to_upload_, submitted_form_, autofill::USERNAME,
      login_form_signature_));
}

// Checks votes uploading when
// 1. User saves a credential on a sign-up, but Chrome picked the wrong
// field as username.
// 2. The user modifies the username on login form before submission.
// Simulates the flow by calling the functions that trigger UploadPasswordVote
// from a level above (FindCorrectedUsernameElement and SendVotesOnSave).
TEST_F(VotesUploaderTest, SendVotesOnSaveOverwrittenFlow) {
  VotesUploader votes_uploader(&client_, false);
  PasswordForm match_form;
  match_form.all_alternative_usernames = {
      {AlternativeElement::Value(u"correct_username"),
       autofill::FieldRendererId(6),
       AlternativeElement::Name(GetFieldNameByIndex(6))}};
  match_form.password_value = u"password_value";
  match_form.times_used_in_html_form = 0;

  for (size_t i = 0; i < 10; ++i) {
    FormFieldData field;
    field.name = GetFieldNameByIndex(i);
    match_form.form_data.fields.push_back(field);
  }
  std::vector<const PasswordForm*> matches = {&match_form};

  ServerFieldTypeSet expected_field_types = {autofill::USERNAME};

  EXPECT_TRUE(votes_uploader.FindCorrectedUsernameElement(
      matches, u"correct_username", u"password_value"));

  // SendVotesOnSave should call UploadPasswordVote and StartUploadRequest
  // twice. The first call is not the one that should be tested.
  testing::Expectation first_call =
      EXPECT_CALL(mock_autofill_crowdsourcing_manager_, StartUploadRequest);

  EXPECT_CALL(
      mock_autofill_crowdsourcing_manager_,
      StartUploadRequest(
          UsernameVoteTypeIsSameAs(
              autofill::AutofillUploadContents::Field::USERNAME_OVERWRITTEN),
          false, expected_field_types, _, true, nullptr, /*observer=*/IsNull()))
      .After(first_call);

  votes_uploader.SendVotesOnSave(form_to_upload_.form_data, submitted_form_,
                                 matches, &form_to_upload_);
}

// Checks votes uploading when user reuses credentials on login form.
TEST_F(VotesUploaderTest, UploadCredentialsReusedVote) {
  VotesUploader votes_uploader(&client_, false);

  form_to_upload_.username_element_renderer_id = FieldRendererId(6);
  form_to_upload_.password_element_renderer_id = FieldRendererId(5);

  form_to_upload_.username_value = u"username_value";
  submitted_form_.username_value = u"username_value";

  std::map<std::u16string, ServerFieldType> expected_types = {
      {GetFieldNameByIndex(6), autofill::USERNAME},
      {GetFieldNameByIndex(5), autofill::ACCOUNT_CREATION_PASSWORD}};
  ServerFieldTypeSet expected_field_types = {
      autofill::ACCOUNT_CREATION_PASSWORD, autofill::USERNAME};

  EXPECT_CALL(
      mock_autofill_crowdsourcing_manager_,
      StartUploadRequest(
          AllOf(
              UploadedAutofillTypesAre(expected_types),
              UsernameVoteTypeIsSameAs(
                  autofill::AutofillUploadContents::Field::CREDENTIALS_REUSED)),
          false, expected_field_types, login_form_signature_, true, nullptr,
          /*observer=*/IsNull()));

  EXPECT_TRUE(votes_uploader.UploadPasswordVote(
      form_to_upload_, submitted_form_, autofill::ACCOUNT_CREATION_PASSWORD,
      login_form_signature_));
}

// Checks votes uploading when user reuses credentials on login form.
// Simulates the flow by calling the function that triggers UploadPasswordVote
// from a level above (SendVoteOnCredentialsReuse).
TEST_F(VotesUploaderTest, SendVoteOnCredentialsReuseFlow) {
  VotesUploader votes_uploader(&client_, false);
  submitted_form_.username_value = u"username_value";

  FormFieldData field;
  field.name = GetFieldNameByIndex(6);
  field.unique_renderer_id = FieldRendererId(6);

  PasswordForm pending;
  pending.times_used_in_html_form = 1;
  pending.username_element_renderer_id = FieldRendererId(6);
  pending.form_data.fields.push_back(field);
  pending.username_value = u"username_value";

  ServerFieldTypeSet expected_field_types = {autofill::USERNAME};

  EXPECT_CALL(
      mock_autofill_crowdsourcing_manager_,
      StartUploadRequest(
          UsernameVoteTypeIsSameAs(
              autofill::AutofillUploadContents::Field::CREDENTIALS_REUSED),
          false, expected_field_types, _, true, nullptr,
          /*observer=*/IsNull()));
  votes_uploader.SendVoteOnCredentialsReuse(form_to_upload_.form_data,
                                            submitted_form_, &pending);
}

// Checks votes uploading when user modifies the username in a prompt.
TEST_F(VotesUploaderTest, UploadUsernameEditedVote) {
  VotesUploader votes_uploader(&client_, false);

  form_to_upload_.username_element_renderer_id = FieldRendererId(6);
  form_to_upload_.password_element_renderer_id = FieldRendererId(5);
  form_to_upload_.username_value = u"new_username_value";

  std::map<std::u16string, ServerFieldType> expected_types = {
      {GetFieldNameByIndex(6), autofill::USERNAME},
      {GetFieldNameByIndex(5), autofill::PASSWORD}};
  ServerFieldTypeSet expected_field_types = {autofill::PASSWORD,
                                             autofill::USERNAME};

  // A user changes the username in a save prompt to the value of
  // another field of the observed form.
  votes_uploader.set_username_change_state(
      VotesUploader::UsernameChangeState::kChangedToKnownValue);

  EXPECT_CALL(
      mock_autofill_crowdsourcing_manager_,
      StartUploadRequest(
          AllOf(UploadedAutofillTypesAre(expected_types),
                UsernameVoteTypeIsSameAs(
                    autofill::AutofillUploadContents::Field::USERNAME_EDITED)),
          false, expected_field_types, login_form_signature_, true, nullptr,
          /*observer=*/IsNull()));

  EXPECT_TRUE(votes_uploader.UploadPasswordVote(
      form_to_upload_, submitted_form_, autofill::PASSWORD,
      login_form_signature_));
}

// Checks votes uploading when user modifies the username in a prompt. Simulates
// the flow by calling the function that triggers UploadPasswordVote from a
// level above (SendVotesOnSave).
// TODO(crbug/1451740): It would be good to simulate the calls triggering
// set_username_change_state (such as UpdatePasswordFormUsernameAndPassword) as
// well.
TEST_F(VotesUploaderTest, SendVotesOnSaveEditedFlow) {
  VotesUploader votes_uploader(&client_, false);

  form_to_upload_.username_element_renderer_id = FieldRendererId(6);
  form_to_upload_.password_element_renderer_id = FieldRendererId(5);
  form_to_upload_.username_value = u"new_username_value";

  ServerFieldTypeSet expected_field_types = {autofill::PASSWORD,
                                             autofill::USERNAME};

  // A user changes the username in a save prompt to the value of
  // another field of the observed form.
  votes_uploader.set_username_change_state(
      VotesUploader::UsernameChangeState::kChangedToKnownValue);

  EXPECT_CALL(mock_autofill_crowdsourcing_manager_,
              StartUploadRequest(
                  UsernameVoteTypeIsSameAs(
                      autofill::AutofillUploadContents::Field::USERNAME_EDITED),
                  false, expected_field_types, _, true, nullptr,
                  /*observer=*/IsNull()));
  votes_uploader.SendVotesOnSave(form_to_upload_.form_data, submitted_form_, {},
                                 &form_to_upload_);
}

TEST_F(VotesUploaderTest, InitialValueDetection) {
  // Tests if the initial value of the (predicted to be the) username field
  // in |form_data| is persistently stored and if it's low-entropy hash is
  // correctly written to the corresponding field in the |form_structure|.
  // Note that the value of the username field is deliberately altered before
  // the |form_structure| is generated from |form_data| to test the persistence.
  std::u16string prefilled_username = u"prefilled_username";
  FieldRendererId username_field_renderer_id(123456);
  const uint32_t kNumberOfHashValues = 64;
  FormData form_data;

  FormFieldData username_field;
  username_field.value = prefilled_username;
  username_field.unique_renderer_id = username_field_renderer_id;

  FormFieldData other_field;
  other_field.value = u"some_field";
  other_field.unique_renderer_id = FieldRendererId(3234);

  form_data.fields = {other_field, username_field};

  VotesUploader votes_uploader(&client_, true);
  votes_uploader.StoreInitialFieldValues(form_data);

  form_data.fields.at(1).value = u"user entered value";
  FormStructure form_structure(form_data);

  PasswordForm password_form;
  password_form.username_element_renderer_id = username_field_renderer_id;

  votes_uploader.SetInitialHashValueOfUsernameField(username_field_renderer_id,
                                                    &form_structure);

  const uint32_t expected_hash = 1377800651 % kNumberOfHashValues;

  int found_fields = 0;
  for (auto& f : form_structure) {
    if (f->unique_renderer_id == username_field_renderer_id) {
      found_fields++;
      ASSERT_TRUE(f->initial_value_hash());
      EXPECT_EQ(f->initial_value_hash().value(), expected_hash);
    }
  }
  EXPECT_EQ(found_fields, 1);
}

// Tests that password attributes are uploaded only if it is the first save or a
// password updated.
TEST_F(VotesUploaderTest, UploadPasswordAttributes) {
  for (const ServerFieldType autofill_type :
       {autofill::PASSWORD, autofill::ACCOUNT_CREATION_PASSWORD,
        autofill::NOT_ACCOUNT_CREATION_PASSWORD, autofill::NEW_PASSWORD,
        autofill::PROBABLY_NEW_PASSWORD, autofill::NOT_NEW_PASSWORD,
        autofill::USERNAME}) {
    SCOPED_TRACE(testing::Message() << "autofill_type=" << autofill_type);
    VotesUploader votes_uploader(&client_, false);
    if (autofill_type == autofill::NEW_PASSWORD ||
        autofill_type == autofill::PROBABLY_NEW_PASSWORD ||
        autofill_type == autofill::NOT_NEW_PASSWORD) {
      form_to_upload_.new_password_element_renderer_id = FieldRendererId(11);
      form_to_upload_.new_password_value = u"new_password_value";
    }

    bool expect_password_attributes = autofill_type == autofill::PASSWORD ||
                                      autofill_type == autofill::NEW_PASSWORD;
    EXPECT_CALL(mock_autofill_crowdsourcing_manager_,
                StartUploadRequest(
                    HasPasswordAttributesVote(expect_password_attributes),
                    false, _, login_form_signature_, true,
                    /* pref_service= */ nullptr,
                    /*observer=*/IsNull()));

    EXPECT_TRUE(votes_uploader.UploadPasswordVote(
        form_to_upload_, submitted_form_, autofill_type,
        login_form_signature_));

    testing::Mock::VerifyAndClearExpectations(
        &mock_autofill_crowdsourcing_manager_);
  }
}

TEST_F(VotesUploaderTest, GeneratePasswordAttributesVote) {
  VotesUploader votes_uploader(&client_, true);
  // Checks that randomization distorts information about present and missed
  // character classes, but a true value is still restorable with aggregation
  // of many distorted reports.
  const char* kPasswordSnippets[kNumberOfPasswordAttributes] = {"abc", "*-_"};
  for (int test_case = 0; test_case < 10; ++test_case) {
    bool has_password_attribute[kNumberOfPasswordAttributes];
    std::u16string password_value;
    for (int i = 0; i < kNumberOfPasswordAttributes; ++i) {
      has_password_attribute[i] = base::RandGenerator(2);
      if (has_password_attribute[i])
        password_value += ASCIIToUTF16(kPasswordSnippets[i]);
    }
    if (password_value.empty())
      continue;

    FormData form;
    FormStructure form_structure(form);
    int reported_false[kNumberOfPasswordAttributes] = {0, 0};
    int reported_true[kNumberOfPasswordAttributes] = {0, 0};

    int reported_actual_length = 0;
    int reported_wrong_length = 0;

    int kNumberOfRuns = 1000;

    for (int i = 0; i < kNumberOfRuns; ++i) {
      votes_uploader.GeneratePasswordAttributesVote(password_value,
                                                    &form_structure);
      std::optional<std::pair<PasswordAttribute, bool>> vote =
          form_structure.get_password_attributes_vote();
      int attribute_index = static_cast<int>(vote->first);
      if (vote->second)
        reported_true[attribute_index]++;
      else
        reported_false[attribute_index]++;
      size_t reported_length = form_structure.get_password_length_vote();
      if (reported_length == password_value.size()) {
        reported_actual_length++;
      } else {
        reported_wrong_length++;
        EXPECT_LT(0u, reported_length);
        EXPECT_LT(reported_length, password_value.size());
      }
    }
    for (int i = 0; i < kNumberOfPasswordAttributes; i++) {
      EXPECT_LT(0, reported_false[i]);
      EXPECT_LT(0, reported_true[i]);

      // If the actual value is |true|, then it should report more |true|s than
      // |false|s.
      if (has_password_attribute[i]) {
        EXPECT_LT(reported_false[i], reported_true[i])
            << "Wrong distribution for attribute " << i
            << ". password_value = " << password_value;
      } else {
        EXPECT_GT(reported_false[i], reported_true[i])
            << "Wrong distribution for attribute " << i
            << ". password_value = " << password_value;
      }
    }
    EXPECT_LT(0, reported_actual_length);
    EXPECT_LT(0, reported_wrong_length);
    EXPECT_LT(reported_actual_length, reported_wrong_length);
  }
}

TEST_F(VotesUploaderTest, GeneratePasswordSpecialSymbolVote) {
  VotesUploader votes_uploader(&client_, true);

  const std::u16string password_value = u"password-withsymbols!";
  const int kNumberOfRuns = 2000;
  const int kSpecialSymbolsAttribute =
      static_cast<int>(PasswordAttribute::kHasSpecialSymbol);

  FormData form;

  int correct_symbol_reported = 0;
  int wrong_symbol_reported = 0;
  int number_of_symbol_votes = 0;

  for (int i = 0; i < kNumberOfRuns; ++i) {
    FormStructure form_structure(form);

    votes_uploader.GeneratePasswordAttributesVote(password_value,
                                                  &form_structure);
    std::optional<std::pair<PasswordAttribute, bool>> vote =
        form_structure.get_password_attributes_vote();

    // Continue if the vote is not about special symbols or implies that no
    // special symbols are used.
    if (static_cast<int>(vote->first) != kSpecialSymbolsAttribute ||
        !vote->second) {
      EXPECT_EQ(form_structure.get_password_symbol_vote(), 0);
      continue;
    }

    number_of_symbol_votes += 1;

    int symbol = form_structure.get_password_symbol_vote();
    if (symbol == '-' || symbol == '!')
      correct_symbol_reported += 1;
    else
      wrong_symbol_reported += 1;
  }
  EXPECT_LT(0.4 * number_of_symbol_votes, correct_symbol_reported);
  EXPECT_LT(0.15 * number_of_symbol_votes, wrong_symbol_reported);
}

TEST_F(VotesUploaderTest, GeneratePasswordAttributesVote_OneCharacterPassword) {
  // |VotesUploader::GeneratePasswordAttributesVote| shouldn't crash if a
  // password has only one character.
  FormData form;
  FormStructure form_structure(form);
  VotesUploader votes_uploader(&client_, true);
  votes_uploader.GeneratePasswordAttributesVote(u"1", &form_structure);
  std::optional<std::pair<PasswordAttribute, bool>> vote =
      form_structure.get_password_attributes_vote();
  EXPECT_TRUE(vote.has_value());
  size_t reported_length = form_structure.get_password_length_vote();
  EXPECT_EQ(1u, reported_length);
}

TEST_F(VotesUploaderTest, GeneratePasswordAttributesVote_AllAsciiCharacters) {
  FormData form;
  FormStructure form_structure(form);
  VotesUploader votes_uploader(&client_, true);
  votes_uploader.GeneratePasswordAttributesVote(
      u"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqr"
      u"stuvwxyz!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~",
      &form_structure);
  std::optional<std::pair<PasswordAttribute, bool>> vote =
      form_structure.get_password_attributes_vote();
  EXPECT_TRUE(vote.has_value());
}

TEST_F(VotesUploaderTest, GeneratePasswordAttributesVote_NonAsciiPassword) {
  // Checks that password attributes vote is not generated if the password has
  // non-ascii characters.
  for (const auto* password :
       {u"пароль1", u"パスワード", u"münchen", u"סיסמה-A", u"Σ-12345",
        u"գաղտնաբառըTTT", u"Slaptažodis", u"密碼", u"كلمهالسر", u"mậtkhẩu!",
        u"ລະຫັດຜ່ານ-l", u"စကားဝှက်ကို3", u"პაროლი", u"पारण शब्द"}) {
    FormData form;
    FormStructure form_structure(form);
    VotesUploader votes_uploader(&client_, true);
    votes_uploader.GeneratePasswordAttributesVote(password, &form_structure);
    std::optional<std::pair<PasswordAttribute, bool>> vote =
        form_structure.get_password_attributes_vote();

    EXPECT_FALSE(vote.has_value()) << password;
  }
}

TEST_F(VotesUploaderTest, NoSingleUsernameDataNoUpload) {
  VotesUploader votes_uploader(&client_, false);
  EXPECT_CALL(mock_autofill_crowdsourcing_manager_, StartUploadRequest)
      .Times(0);
  base::HistogramTester histogram_tester;
  votes_uploader.set_should_send_username_first_flow_votes(true);
  votes_uploader.MaybeSendSingleUsernameVotes();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.SingleUsername.VoteDataAvailability",
      static_cast<int>(
          VotesUploader::SingleUsernameVoteDataAvailability::kNone),
      1);
}

TEST_F(VotesUploaderTest, UploadSingleUsernameMultipleFieldsInUsernameForm) {
  VotesUploader votes_uploader(&client_, false);

  // Make form predictions for a form with multiple fields.
  FormPredictions form_predictions;
  form_predictions.form_signature = kSingleUsernameFormSignature;
  // Add a non-username field.
  form_predictions.fields.emplace_back();
  form_predictions.fields.back().renderer_id.value() =
      kSingleUsernameRendererId.value() - 1;
  form_predictions.fields.back().signature.value() =
      kSingleUsernameFieldSignature.value() - 1;
  // Add the username field.
  form_predictions.fields.emplace_back();
  form_predictions.fields.back().renderer_id = kSingleUsernameRendererId;
  form_predictions.fields.back().signature = kSingleUsernameFieldSignature;

  std::u16string single_username_candidate_value = u"username_candidate_value";
  votes_uploader.add_single_username_vote_data(SingleUsernameVoteData(
      kSingleUsernameRendererId, single_username_candidate_value,
      form_predictions,
      /*stored_credentials=*/{}, PasswordFormHadMatchingUsername(false)));
  votes_uploader.set_suggested_username(single_username_candidate_value);
  votes_uploader.CalculateUsernamePromptEditState(
      /*saved_username=*/single_username_candidate_value,
      /*all_alternative_usernames=*/{});
  votes_uploader.set_should_send_username_first_flow_votes(true);

#if !BUILDFLAG(IS_ANDROID)
  // Upload on the username form.
  ServerFieldTypeSet expected_types = {SINGLE_USERNAME};
  EXPECT_CALL(mock_autofill_crowdsourcing_manager_,
              StartUploadRequest(
                  AllOf(SignatureIs(kSingleUsernameFormSignature),
                        UploadedSingleUsernameVoteTypeIs(
                            autofill::AutofillUploadContents::Field::WEAK)),
                  false, expected_types, std::string(), true,
                  /* pref_service= */ nullptr, /*observer=*/IsNull()));
#else
  EXPECT_CALL(mock_autofill_crowdsourcing_manager_, StartUploadRequest)
      .Times(0);
#endif  // !BUILDFLAG(IS_ANDROID)

  base::HistogramTester histogram_tester;
  votes_uploader.MaybeSendSingleUsernameVotes();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.SingleUsername.VoteDataAvailability",
      static_cast<int>(VotesUploader::SingleUsernameVoteDataAvailability::
                           kUsernameFirstOnly),
      1);
}

// Tests that a negeative vote is sent if the username candidate field
// value contained whitespaces.
TEST_F(VotesUploaderTest, UploadNotSingleUsernameForWhitespaces) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kUsernameFirstFlowFallbackCrowdsourcing);

  VotesUploader votes_uploader(&client_, false);
  votes_uploader.add_single_username_vote_data(SingleUsernameVoteData(
      kSingleUsernameRendererId,
      /*username_value=*/u"some search query",
      MakeSimpleSingleUsernamePredictions(),
      /*stored_credentials=*/{}, PasswordFormHadMatchingUsername(false)));
  votes_uploader.CalculateUsernamePromptEditState(
      /*saved_username=*/u"saved_value", /*all_alternative_usernames=*/{});
  votes_uploader.set_should_send_username_first_flow_votes(true);

#if !BUILDFLAG(IS_ANDROID)
  // Upload on the username form.
  ServerFieldTypeSet expected_types = {NOT_USERNAME};
  EXPECT_CALL(mock_autofill_crowdsourcing_manager_,
              StartUploadRequest(
                  AllOf(SignatureIs(kSingleUsernameFormSignature),
                        UploadedSingleUsernameVoteTypeIs(
                            autofill::AutofillUploadContents::Field::STRONG)),
                  false, expected_types, std::string(), true,
                  /* pref_service= */ nullptr, /*observer=*/IsNull()));
#else
  EXPECT_CALL(mock_autofill_crowdsourcing_manager_, StartUploadRequest)
      .Times(0);
#endif  // !BUILDFLAG(IS_ANDROID)

  votes_uploader.MaybeSendSingleUsernameVotes();

  // Upload on the password form for the fallback classifier.
  autofill::AutofillUploadContents::SingleUsernameData
      expected_single_username_data = MakeSimpleSingleUsernameData();
  expected_single_username_data.set_value_type(
      autofill::AutofillUploadContents::VALUE_WITH_WHITESPACE);
  expected_single_username_data.set_prompt_edit(
      autofill::AutofillUploadContents::EDITED_NEGATIVE);
  EXPECT_CALL(
      mock_autofill_crowdsourcing_manager_,
      StartUploadRequest(
          AllOf(SignatureIsSameAs(submitted_form_),
                UploadedSingleUsernameDataIs({expected_single_username_data})),
          _, _, _, _, _, /*observer=*/IsNull()));

  votes_uploader.UploadPasswordVote(submitted_form_, submitted_form_,
                                    autofill::PASSWORD, std::string());
}

// Verifies that SINGLE_USERNAME vote and NOT_EDITED_IN_PROMPT vote type
// are sent if single username candidate value was suggested and accepted.
TEST_F(VotesUploaderTest, SingleUsernameValueSuggestedAndAccepted) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kUsernameFirstFlowFallbackCrowdsourcing);

  VotesUploader votes_uploader(&client_, false);
  std::u16string single_username_candidate_value = u"username_candidate_value";
  votes_uploader.add_single_username_vote_data(SingleUsernameVoteData(
      kSingleUsernameRendererId, single_username_candidate_value,
      MakeSimpleSingleUsernamePredictions(), /*stored_credentials=*/{},
      PasswordFormHadMatchingUsername(false)));
  votes_uploader.set_suggested_username(single_username_candidate_value);
  votes_uploader.CalculateUsernamePromptEditState(
      /*saved_username=*/single_username_candidate_value,
      /*all_alternative_usernames=*/{});
  votes_uploader.set_should_send_username_first_flow_votes(true);

#if !BUILDFLAG(IS_ANDROID)
  // Upload on the username form.
  ServerFieldTypeSet expected_types = {SINGLE_USERNAME};
  EXPECT_CALL(mock_autofill_crowdsourcing_manager_,
              StartUploadRequest(
                  AllOf(SignatureIs(kSingleUsernameFormSignature),
                        UploadedSingleUsernameVoteTypeIs(
                            autofill::AutofillUploadContents::Field::WEAK)),
                  /*form_was_autofilled=*/false, expected_types,
                  /*login_form_signature=*/"",
                  /*observed_submission=*/true,
                  /*pref_service=*/nullptr,
                  /*observer=*/IsNull()));
#else
  EXPECT_CALL(mock_autofill_crowdsourcing_manager_, StartUploadRequest)
      .Times(0);
#endif  // !BUILDFLAG(IS_ANDROID)

  votes_uploader.MaybeSendSingleUsernameVotes();

  // Upload on the password form for the fallback classifier.
  autofill::AutofillUploadContents::SingleUsernameData
      expected_single_username_data = MakeSimpleSingleUsernameData();
  expected_single_username_data.set_prompt_edit(
      autofill::AutofillUploadContents::NOT_EDITED_POSITIVE);
  EXPECT_CALL(
      mock_autofill_crowdsourcing_manager_,
      StartUploadRequest(
          AllOf(SignatureIsSameAs(submitted_form_),
                UploadedSingleUsernameDataIs({expected_single_username_data})),
          _, _, _, _, _, /*observer=*/IsNull()));

  votes_uploader.UploadPasswordVote(submitted_form_, submitted_form_,
                                    autofill::PASSWORD, std::string());
}

// Verifies that NOT_USERNAME vote and NOT_EDITED_IN_PROMPT vote type
// are sent if value other than single username candidate was suggested and
// accepted.
TEST_F(VotesUploaderTest, SingleUsernameOtherValueSuggestedAndAccepted) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kUsernameFirstFlowFallbackCrowdsourcing);

  VotesUploader votes_uploader(&client_, false);
  std::u16string single_username_candidate_value = u"username_candidate_value";
  votes_uploader.add_single_username_vote_data(SingleUsernameVoteData(
      kSingleUsernameRendererId, single_username_candidate_value,
      MakeSimpleSingleUsernamePredictions(), /*stored_credentials=*/{},
      PasswordFormHadMatchingUsername(false)));
  std::u16string suggested_value = u"other_value";
  votes_uploader.set_suggested_username(suggested_value);
  votes_uploader.CalculateUsernamePromptEditState(
      /*saved_username=*/suggested_value, /*all_alternative_usernames=*/{});
  votes_uploader.set_should_send_username_first_flow_votes(true);

#if !BUILDFLAG(IS_ANDROID)
  // Upload on the username form.
  ServerFieldTypeSet expected_types = {NOT_USERNAME};
  EXPECT_CALL(mock_autofill_crowdsourcing_manager_,
              StartUploadRequest(
                  AllOf(SignatureIs(kSingleUsernameFormSignature),
                        UploadedSingleUsernameVoteTypeIs(
                            autofill::AutofillUploadContents::Field::WEAK)),
                  /*form_was_autofilled=*/false, expected_types,
                  /*login_form_signature=*/"",
                  /*observed_submission=*/true,
                  /*pref_service=*/nullptr,
                  /*observer=*/IsNull()));
#else
  EXPECT_CALL(mock_autofill_crowdsourcing_manager_, StartUploadRequest)
      .Times(0);
#endif  // !BUILDFLAG(IS_ANDROID)

  votes_uploader.MaybeSendSingleUsernameVotes();

  // Upload on the password form for the fallback classifier.
  autofill::AutofillUploadContents::SingleUsernameData
      expected_single_username_data = MakeSimpleSingleUsernameData();
  expected_single_username_data.set_prompt_edit(
      autofill::AutofillUploadContents::NOT_EDITED_NEGATIVE);
  EXPECT_CALL(
      mock_autofill_crowdsourcing_manager_,
      StartUploadRequest(
          AllOf(SignatureIsSameAs(submitted_form_),
                UploadedSingleUsernameDataIs({expected_single_username_data})),
          _, _, _, _, _, /*observer=*/IsNull()));
  votes_uploader.UploadPasswordVote(submitted_form_, submitted_form_,
                                    autofill::PASSWORD, std::string());
}

// Verifies that SINGLE_USERNAME vote and EDITED_IN_PROMPT vote type are sent
// if value other than single username candidate was suggested, but the user
// has inputted single username candidate value in prompt.
TEST_F(VotesUploaderTest, SingleUsernameValueSetInPrompt) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kUsernameFirstFlowFallbackCrowdsourcing);

  VotesUploader votes_uploader(&client_, false);
  std::u16string single_username_candidate_value = u"username_candidate_value";
  votes_uploader.add_single_username_vote_data(SingleUsernameVoteData(
      kSingleUsernameRendererId, single_username_candidate_value,
      MakeSimpleSingleUsernamePredictions(), /*stored_credentials=*/{},
      PasswordFormHadMatchingUsername(false)));
  std::u16string suggested_value = u"other_value";
  votes_uploader.set_suggested_username(suggested_value);
  votes_uploader.CalculateUsernamePromptEditState(
      /*saved_username=*/single_username_candidate_value,
      /*all_alternative_usernames=*/{});
  votes_uploader.set_should_send_username_first_flow_votes(true);

#if !BUILDFLAG(IS_ANDROID)
  ServerFieldTypeSet expected_types = {SINGLE_USERNAME};
  EXPECT_CALL(mock_autofill_crowdsourcing_manager_,
              StartUploadRequest(
                  AllOf(SignatureIs(kSingleUsernameFormSignature),
                        UploadedSingleUsernameVoteTypeIs(
                            autofill::AutofillUploadContents::Field::STRONG)),
                  /*form_was_autofilled=*/false, expected_types,
                  /*login_form_signature=*/"",
                  /*observed_submission=*/true,
                  /*pref_service=*/nullptr,
                  /*observer=*/IsNull()));
#else
  EXPECT_CALL(mock_autofill_crowdsourcing_manager_, StartUploadRequest)
      .Times(0);
#endif  // !BUILDFLAG(IS_ANDROID)

  votes_uploader.MaybeSendSingleUsernameVotes();

  // Upload on the password form for the fallback classifier.
  autofill::AutofillUploadContents::SingleUsernameData
      expected_single_username_data = MakeSimpleSingleUsernameData();
  expected_single_username_data.set_prompt_edit(
      autofill::AutofillUploadContents::EDITED_POSITIVE);
  EXPECT_CALL(
      mock_autofill_crowdsourcing_manager_,
      StartUploadRequest(
          AllOf(SignatureIsSameAs(submitted_form_),
                UploadedSingleUsernameDataIs({expected_single_username_data})),
          _, _, _, _, _, /*observer=*/IsNull()));
  votes_uploader.UploadPasswordVote(submitted_form_, submitted_form_,
                                    autofill::PASSWORD, std::string());
}

// Verifies that NOT_USERNAME vote and EDITED_IN_PROMPT vote type are sent
// if single username candidate value was suggested, but the user has deleted
// it in prompt.
TEST_F(VotesUploaderTest, SingleUsernameValueDeletedInPrompt) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kUsernameFirstFlowFallbackCrowdsourcing);

  VotesUploader votes_uploader(&client_, false);
  std::u16string single_username_candidate_value = u"username_candidate_value";
  votes_uploader.add_single_username_vote_data(SingleUsernameVoteData(
      kSingleUsernameRendererId, single_username_candidate_value,
      MakeSimpleSingleUsernamePredictions(), /*stored_credentials=*/{},
      PasswordFormHadMatchingUsername(false)));
  votes_uploader.set_suggested_username(single_username_candidate_value);
  votes_uploader.CalculateUsernamePromptEditState(
      /*saved_username=*/u"", /*all_alternative_usernames=*/{});
  votes_uploader.set_should_send_username_first_flow_votes(true);

#if !BUILDFLAG(IS_ANDROID)
  // Upload on the username form.
  ServerFieldTypeSet expected_types = {NOT_USERNAME};
  EXPECT_CALL(mock_autofill_crowdsourcing_manager_,
              StartUploadRequest(
                  AllOf(SignatureIs(kSingleUsernameFormSignature),
                        UploadedSingleUsernameVoteTypeIs(
                            autofill::AutofillUploadContents::Field::STRONG)),
                  /*form_was_autofilled=*/false, expected_types,
                  /*login_form_signature=*/"",
                  /*observed_submission=*/true,
                  /*pref_service=*/nullptr,
                  /*observer=*/IsNull()));
#else
  EXPECT_CALL(mock_autofill_crowdsourcing_manager_, StartUploadRequest)
      .Times(0);
#endif  // !BUILDFLAG(IS_ANDROID)

  votes_uploader.MaybeSendSingleUsernameVotes();

  // Expect upload for the password form for the fallback classifier.
  autofill::AutofillUploadContents::SingleUsernameData
      expected_single_username_data = MakeSimpleSingleUsernameData();
  expected_single_username_data.set_prompt_edit(
      autofill::AutofillUploadContents::EDITED_NEGATIVE);
  EXPECT_CALL(
      mock_autofill_crowdsourcing_manager_,
      StartUploadRequest(
          AllOf(SignatureIsSameAs(submitted_form_),
                UploadedSingleUsernameDataIs({expected_single_username_data})),
          _, _, _, _, _, /*observer=*/IsNull()));
  votes_uploader.UploadPasswordVote(submitted_form_, submitted_form_,
                                    autofill::PASSWORD, std::string());
}

// Verifies that no vote is sent if the user has deleted the username value
// suggested in prompt, and suggested value wasn't equal to single username
// candidate value.
TEST_F(VotesUploaderTest, NotSingleUsernameValueDeletedInPrompt) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kUsernameFirstFlowFallbackCrowdsourcing);

  VotesUploader votes_uploader(&client_, false);
  std::u16string single_username_candidate_value = u"username_candidate_value";
  votes_uploader.add_single_username_vote_data(SingleUsernameVoteData(
      kSingleUsernameRendererId, single_username_candidate_value,
      MakeSimpleSingleUsernamePredictions(), /*stored_credentials=*/{},
      PasswordFormHadMatchingUsername(false)));
  std::u16string other_value = u"other_value";
  votes_uploader.set_suggested_username(other_value);
  votes_uploader.CalculateUsernamePromptEditState(
      /*saved_username=*/u"", /*all_alternative_usernames=*/{});
  votes_uploader.set_should_send_username_first_flow_votes(true);

  // Expect no upload on username form, as th signal is not informative to us.
  EXPECT_CALL(mock_autofill_crowdsourcing_manager_,
              StartUploadRequest(SignatureIs(kSingleUsernameFormSignature), _,
                                 _, _, _, _, /*observer=*/IsNull()))
      .Times(0);
  votes_uploader.MaybeSendSingleUsernameVotes();

  // Expect upload for the password form for the fallback classifier.
  autofill::AutofillUploadContents::SingleUsernameData
      expected_single_username_data = MakeSimpleSingleUsernameData();
  expected_single_username_data.set_prompt_edit(
      autofill::AutofillUploadContents::EDIT_UNSPECIFIED);
  EXPECT_CALL(
      mock_autofill_crowdsourcing_manager_,
      StartUploadRequest(
          AllOf(SignatureIsSameAs(submitted_form_),
                UploadedSingleUsernameDataIs({expected_single_username_data})),
          _, _, _, _, _, /*observer=*/IsNull()));
  votes_uploader.UploadPasswordVote(submitted_form_, submitted_form_,
                                    autofill::PASSWORD, std::string());
}

// Verifies that NOT_USERNAME vote is sent on password form if no single
// username typing had preceded single password typing.
TEST_F(VotesUploaderTest, SingleUsernameNoUsernameCandidate) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kUsernameFirstFlowFallbackCrowdsourcing);

  VotesUploader votes_uploader(&client_, false);
  votes_uploader.add_single_username_vote_data(SingleUsernameVoteData());
  votes_uploader.set_suggested_username(u"");
  votes_uploader.CalculateUsernamePromptEditState(
      /*saved_username=*/u"", /*all_alternative_usernames=*/{});
  votes_uploader.set_should_send_username_first_flow_votes(true);

  votes_uploader.MaybeSendSingleUsernameVotes();

  // Expect upload on the password form for the fallback classifier.
  autofill::AutofillUploadContents::SingleUsernameData
      expected_single_username_data;
  expected_single_username_data.set_username_form_signature(0);
  expected_single_username_data.set_username_field_signature(0);
  expected_single_username_data.set_value_type(
      autofill::AutofillUploadContents::NO_VALUE_TYPE);

  EXPECT_CALL(
      mock_autofill_crowdsourcing_manager_,
      StartUploadRequest(
          AllOf(SignatureIsSameAs(submitted_form_),
                UploadedSingleUsernameDataIs({expected_single_username_data})),
          _, _, _, _, _, /*observer=*/IsNull()));
  votes_uploader.UploadPasswordVote(submitted_form_, submitted_form_,
                                    autofill::PASSWORD, std::string());
}

// Tests FieldNameCollisionInVotes metric doesn't report "true" when multiple
// fields in the form to be uploaded have the same name.
TEST_F(VotesUploaderTest, FieldNameCollisionInVotes) {
  VotesUploader votes_uploader(&client_, false);
  std::u16string password_element = GetFieldNameByIndex(5);
  form_to_upload_.password_element = password_element;
  form_to_upload_.password_element_renderer_id = FieldRendererId(5);
  submitted_form_.password_element = password_element;
  submitted_form_.password_element_renderer_id = FieldRendererId(5);
  form_to_upload_.confirmation_password_element = password_element;
  form_to_upload_.confirmation_password_element_renderer_id =
      FieldRendererId(11);
  submitted_form_.confirmation_password_element = password_element;
  submitted_form_.confirmation_password_element_renderer_id =
      FieldRendererId(11);
  ServerFieldTypeSet expected_field_types = {PASSWORD, CONFIRMATION_PASSWORD};

  EXPECT_CALL(mock_autofill_crowdsourcing_manager_,
              StartUploadRequest(_, false, expected_field_types,
                                 login_form_signature_, true,
                                 /* pref_service= */ nullptr,
                                 /*observer=*/IsNull()));
  base::HistogramTester histogram_tester;

  EXPECT_TRUE(votes_uploader.UploadPasswordVote(
      form_to_upload_, submitted_form_, PASSWORD, login_form_signature_));

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.FieldNameCollisionInVotes", false, 1);
}

// Tests FieldNameCollisionInVotes metric reports "false" when all fields in the
// form to be uploaded have different names.
TEST_F(VotesUploaderTest, NoFieldNameCollisionInVotes) {
  VotesUploader votes_uploader(&client_, false);
  form_to_upload_.password_element_renderer_id = FieldRendererId(5);
  submitted_form_.password_element_renderer_id = FieldRendererId(5);
  form_to_upload_.confirmation_password_element_renderer_id =
      FieldRendererId(12);
  submitted_form_.confirmation_password_element_renderer_id =
      FieldRendererId(12);
  ServerFieldTypeSet expected_field_types = {PASSWORD, CONFIRMATION_PASSWORD};

  EXPECT_CALL(mock_autofill_crowdsourcing_manager_,
              StartUploadRequest(_, false, expected_field_types,
                                 login_form_signature_, true,
                                 /* pref_service= */ nullptr,
                                 /*observer=*/IsNull()));
  base::HistogramTester histogram_tester;

  EXPECT_TRUE(votes_uploader.UploadPasswordVote(
      form_to_upload_, submitted_form_, PASSWORD, login_form_signature_));

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.FieldNameCollisionInVotes", false, 1);
}

TEST_F(VotesUploaderTest, ForgotPasswordFormVote) {
  VotesUploader votes_uploader(&client_, false);
  std::u16string single_username_candidate_value = u"username_candidate_value";
  votes_uploader.AddForgotPasswordVoteData(SingleUsernameVoteData(
      kSingleUsernameRendererId, single_username_candidate_value,
      MakeSimpleSingleUsernamePredictions(), /*stored_credentials=*/{},
      PasswordFormHadMatchingUsername(false)));
  votes_uploader.set_suggested_username(single_username_candidate_value);
  votes_uploader.CalculateUsernamePromptEditState(
      /*saved_username=*/single_username_candidate_value,
      /*all_alternative_usernames=*/{});

  // Upload on the username form.
  ServerFieldTypeSet expected_types = {
      autofill::SINGLE_USERNAME_FORGOT_PASSWORD};
  EXPECT_CALL(
      mock_autofill_crowdsourcing_manager_,
      StartUploadRequest(AllOf(SignatureIs(kSingleUsernameFormSignature),
                               UploadedSingleUsernameVoteTypeIs(
                                   autofill::AutofillUploadContents::Field::
                                       WEAK_FORGOT_PASSWORD)),
                         /*form_was_autofilled=*/false, expected_types,
                         /*login_form_signature=*/"",
                         /*observed_submission=*/true,
                         /*pref_service=*/nullptr,
                         /*observer=*/IsNull()));

  base::HistogramTester histogram_tester;
  votes_uploader.MaybeSendSingleUsernameVotes();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.SingleUsername.VoteDataAvailability",
      static_cast<int>(VotesUploader::SingleUsernameVoteDataAvailability::
                           kForgotPasswordOnly),
      1);
}

// Tests "PasswordManager.SingleUsername.VoteDataAvailability" UMA recording
// when both UFF and FPF data is available and has info about the same form.
TEST_F(VotesUploaderTest, SingleUsernameVoteDataUffOverlapsWithFpf) {
  VotesUploader votes_uploader(&client_, false);

  SingleUsernameVoteData data(kSingleUsernameRendererId, u"possible_username",
                              MakeSimpleSingleUsernamePredictions(),
                              /*stored_credentials=*/{},
                              PasswordFormHadMatchingUsername(false));

  votes_uploader.add_single_username_vote_data(data);
  votes_uploader.AddForgotPasswordVoteData(data);

  base::HistogramTester histogram_tester;
  votes_uploader.MaybeSendSingleUsernameVotes();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.SingleUsername.VoteDataAvailability",
      static_cast<int>(
          VotesUploader::SingleUsernameVoteDataAvailability::kBothWithOverlap),
      1);
}

// Tests "PasswordManager.SingleUsername.VoteDataAvailability" UMA recording
// when both UFF and FPF data is available and has info about different forms.
TEST_F(VotesUploaderTest, SingleUsernameVoteDataUffNoOverlapWithFpf) {
  VotesUploader votes_uploader(&client_, false);

  SingleUsernameVoteData data1(FieldRendererId(100), u"maybe_username",
                               MakeSimpleSingleUsernamePredictions(),
                               /*stored_credentials=*/{},
                               PasswordFormHadMatchingUsername(false));
  votes_uploader.add_single_username_vote_data(data1);

  SingleUsernameVoteData data2(FieldRendererId(200), u"also_maybe_username",
                               MakeSimpleSingleUsernamePredictions(),
                               /*stored_credentials=*/{},
                               PasswordFormHadMatchingUsername(false));
  votes_uploader.AddForgotPasswordVoteData(data2);

  base::HistogramTester histogram_tester;
  votes_uploader.MaybeSendSingleUsernameVotes();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.SingleUsername.VoteDataAvailability",
      static_cast<int>(
          VotesUploader::SingleUsernameVoteDataAvailability::kBothNoOverlap),
      1);
}

}  // namespace password_manager
