// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/votes_uploader.h"

#include <string>
#include <utility>

#include "base/hash/hash.h"
#include "base/optional.h"
#include "base/rand_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_download_manager.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/renderer_id.h"
#include "components/autofill/core/common/signatures.h"
#include "components/password_manager/core/browser/field_info_manager.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/vote_uploads_test_matchers.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::AutofillDownloadManager;
using autofill::CONFIRMATION_PASSWORD;
using autofill::FieldSignature;
using autofill::FormData;
using autofill::FormFieldData;
using autofill::FormSignature;
using autofill::FormStructure;
using autofill::NEW_PASSWORD;
using autofill::NOT_USERNAME;
using autofill::PASSWORD;
using autofill::PasswordAttribute;
using autofill::ServerFieldType;
using autofill::ServerFieldTypeSet;
using autofill::SINGLE_USERNAME;
using autofill::UNKNOWN_TYPE;
using autofill::mojom::SubmissionIndicatorEvent;
using base::ASCIIToUTF16;
using testing::_;
using testing::AllOf;
using testing::AnyNumber;
using testing::Return;
using testing::SaveArg;

namespace password_manager {
namespace {

MATCHER_P3(FieldInfoHasData, form_signature, field_signature, field_type, "") {
  return arg.form_signature == form_signature &&
         arg.field_signature == field_signature &&
         arg.field_type == field_type && arg.create_time != base::Time();
}

constexpr int kNumberOfPasswordAttributes =
    static_cast<int>(PasswordAttribute::kPasswordAttributesCount);

class MockAutofillDownloadManager : public AutofillDownloadManager {
 public:
  MockAutofillDownloadManager()
      : AutofillDownloadManager(nullptr, &fake_observer) {}

  MOCK_METHOD6(StartUploadRequest,
               bool(const FormStructure&,
                    bool,
                    const ServerFieldTypeSet&,
                    const std::string&,
                    bool,
                    PrefService*));

 private:
  class StubObserver : public AutofillDownloadManager::Observer {
    void OnLoadedServerPredictions(
        std::string response,
        const std::vector<autofill::FormSignature>& form_signatures) override {}
  };

  StubObserver fake_observer;
  DISALLOW_COPY_AND_ASSIGN(MockAutofillDownloadManager);
};

class MockPasswordManagerClient : public StubPasswordManagerClient {
 public:
  MOCK_METHOD0(GetAutofillDownloadManager, AutofillDownloadManager*());
  MOCK_CONST_METHOD0(GetFieldInfoManager, FieldInfoManager*());
};

class MockFieldInfoManager : public FieldInfoManager {
 public:
  MOCK_METHOD(void,
              AddFieldType,
              (FormSignature, FieldSignature, ServerFieldType),
              (override));
  MOCK_METHOD(ServerFieldType,
              GetFieldType,
              (FormSignature, autofill::FieldSignature),
              (const override));
};

}  // namespace

class VotesUploaderTest : public testing::Test {
 public:
  VotesUploaderTest() {
    EXPECT_CALL(client_, GetAutofillDownloadManager())
        .WillRepeatedly(Return(&mock_autofill_download_manager_));

    ON_CALL(mock_autofill_download_manager_,
            StartUploadRequest(_, _, _, _, _, _))
        .WillByDefault(Return(true));

    // Create |fields| in |form_to_upload_| and |submitted_form_|. Only |name|
    // field in FormFieldData is important. Set them to the unique values based
    // on index.
    const size_t kNumberOfFields = 20;
    for (size_t i = 0; i < kNumberOfFields; ++i) {
      FormFieldData field;
      field.name = GetFieldNameByIndex(i);
      form_to_upload_.form_data.fields.push_back(field);
      submitted_form_.form_data.fields.push_back(field);
    }
  }

 protected:
  base::string16 GetFieldNameByIndex(size_t index) {
    return ASCIIToUTF16("field") + base::NumberToString16(index);
  }

  base::test::TaskEnvironment task_environment_;
  MockAutofillDownloadManager mock_autofill_download_manager_;

  MockPasswordManagerClient client_;

  PasswordForm form_to_upload_;
  PasswordForm submitted_form_;

  std::string login_form_signature_ = "123";
};

TEST_F(VotesUploaderTest, UploadPasswordVoteUpdate) {
  VotesUploader votes_uploader(&client_, true);
  base::string16 new_password_element = GetFieldNameByIndex(3);
  base::string16 confirmation_element = GetFieldNameByIndex(11);
  form_to_upload_.new_password_element = new_password_element;
  submitted_form_.new_password_element = new_password_element;
  form_to_upload_.confirmation_password_element = confirmation_element;
  submitted_form_.confirmation_password_element = confirmation_element;
  submitted_form_.submission_event =
      SubmissionIndicatorEvent::HTML_FORM_SUBMISSION;
  ServerFieldTypeSet expected_field_types = {NEW_PASSWORD,
                                             CONFIRMATION_PASSWORD};
  FieldTypeMap expected_types = {{new_password_element, NEW_PASSWORD},
                                 {confirmation_element, CONFIRMATION_PASSWORD}};
  SubmissionIndicatorEvent expected_submission_event =
      SubmissionIndicatorEvent::HTML_FORM_SUBMISSION;

  EXPECT_CALL(
      mock_autofill_download_manager_,
      StartUploadRequest(
          AllOf(SignatureIsSameAs(form_to_upload_),
                UploadedAutofillTypesAre(expected_types),
                SubmissionEventIsSameAs(expected_submission_event)),
          false, expected_field_types, login_form_signature_, true, nullptr));

  EXPECT_TRUE(votes_uploader.UploadPasswordVote(
      form_to_upload_, submitted_form_, NEW_PASSWORD, login_form_signature_));
}

TEST_F(VotesUploaderTest, UploadPasswordVoteSave) {
  VotesUploader votes_uploader(&client_, false);
  base::string16 password_element = GetFieldNameByIndex(5);
  base::string16 confirmation_element = GetFieldNameByIndex(12);
  form_to_upload_.password_element = password_element;
  submitted_form_.password_element = password_element;
  form_to_upload_.confirmation_password_element = confirmation_element;
  submitted_form_.confirmation_password_element = confirmation_element;
  submitted_form_.submission_event =
      SubmissionIndicatorEvent::HTML_FORM_SUBMISSION;
  ServerFieldTypeSet expected_field_types = {PASSWORD, CONFIRMATION_PASSWORD};
  SubmissionIndicatorEvent expected_submission_event =
      SubmissionIndicatorEvent::HTML_FORM_SUBMISSION;

  EXPECT_CALL(mock_autofill_download_manager_,
              StartUploadRequest(
                  SubmissionEventIsSameAs(expected_submission_event), false,
                  expected_field_types, login_form_signature_, true,
                  /* pref_service= */ nullptr));

  EXPECT_TRUE(votes_uploader.UploadPasswordVote(
      form_to_upload_, submitted_form_, PASSWORD, login_form_signature_));
}

TEST_F(VotesUploaderTest, InitialValueDetection) {
  // Tests if the initial value of the (predicted to be the) username field
  // in |form_data| is persistently stored and if it's low-entropy hash is
  // correctly written to the corresponding field in the |form_structure|.
  // Note that the value of the username field is deliberately altered before
  // the |form_structure| is generated from |form_data| to test the persistence.
  base::string16 prefilled_username = ASCIIToUTF16("prefilled_username");
  autofill::FieldRendererId username_field_renderer_id(123456);
  const uint32_t kNumberOfHashValues = 64;
  FormData form_data;

  FormFieldData username_field;
  username_field.value = prefilled_username;
  username_field.unique_renderer_id = username_field_renderer_id;

  FormFieldData other_field;
  other_field.value = ASCIIToUTF16("some_field");
  other_field.unique_renderer_id = autofill::FieldRendererId(3234);

  form_data.fields = {other_field, username_field};

  VotesUploader votes_uploader(&client_, true);
  votes_uploader.StoreInitialFieldValues(form_data);

  form_data.fields.at(1).value = ASCIIToUTF16("user entered value");
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

TEST_F(VotesUploaderTest, GeneratePasswordAttributesVote) {
  VotesUploader votes_uploader(&client_, true);
  // Checks that randomization distorts information about present and missed
  // character classes, but a true value is still restorable with aggregation
  // of many distorted reports.
  const char* kPasswordSnippets[kNumberOfPasswordAttributes] = {"abc", "*-_"};
  for (int test_case = 0; test_case < 10; ++test_case) {
    bool has_password_attribute[kNumberOfPasswordAttributes];
    base::string16 password_value;
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
      base::Optional<std::pair<PasswordAttribute, bool>> vote =
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

  const base::string16 password_value = ASCIIToUTF16("password-withsymbols!");
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
    base::Optional<std::pair<PasswordAttribute, bool>> vote =
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
  votes_uploader.GeneratePasswordAttributesVote(ASCIIToUTF16("1"),
                                                &form_structure);
  base::Optional<std::pair<PasswordAttribute, bool>> vote =
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
      base::UTF8ToUTF16("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqr"
                        "stuvwxyz!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~"),
      &form_structure);
  base::Optional<std::pair<PasswordAttribute, bool>> vote =
      form_structure.get_password_attributes_vote();
  EXPECT_TRUE(vote.has_value());
}

TEST_F(VotesUploaderTest, GeneratePasswordAttributesVote_NonAsciiPassword) {
  // Checks that password attributes vote is not generated if the password has
  // non-ascii characters.
  for (const auto* password :
       {"пароль1", "パスワード", "münchen", "סיסמה-A", "Σ-12345",
        "գաղտնաբառըTTT", "Slaptažodis", "密碼", "كلمهالسر", "mậtkhẩu!",
        "ລະຫັດຜ່ານ-l", "စကားဝှက်ကို3", "პაროლი", "पारण शब्द"}) {
    FormData form;
    FormStructure form_structure(form);
    VotesUploader votes_uploader(&client_, true);
    votes_uploader.GeneratePasswordAttributesVote(base::UTF8ToUTF16(password),
                                                  &form_structure);
    base::Optional<std::pair<PasswordAttribute, bool>> vote =
        form_structure.get_password_attributes_vote();

    EXPECT_FALSE(vote.has_value()) << password;
  }
}

TEST_F(VotesUploaderTest, NoSingleUsernameDataNoUpload) {
  VotesUploader votes_uploader(&client_, false);
  EXPECT_CALL(mock_autofill_download_manager_,
              StartUploadRequest(_, _, _, _, _, _))
      .Times(0);
  votes_uploader.MaybeSendSingleUsernameVote(true /* credentials_saved */);
}

TEST_F(VotesUploaderTest, UploadSingleUsername) {
  for (bool credentials_saved : {false, true}) {
    SCOPED_TRACE(testing::Message("credentials_saved = ") << credentials_saved);
    VotesUploader votes_uploader(&client_, false);

    MockFieldInfoManager mock_field_manager;
    ON_CALL(mock_field_manager, GetFieldType(_, _))
        .WillByDefault(Return(UNKNOWN_TYPE));
    ON_CALL(client_, GetFieldInfoManager())
        .WillByDefault(Return(&mock_field_manager));

    constexpr autofill::FieldRendererId kUsernameRendererId(101);
    constexpr FieldSignature kUsernameFieldSignature(1234);
    constexpr FormSignature kFormSignature(1000);

    FormPredictions form_predictions;
    form_predictions.form_signature = kFormSignature;
    // Add a non-username field.
    form_predictions.fields.emplace_back();
    form_predictions.fields.back().renderer_id.value() =
        kUsernameRendererId.value() - 1;
    form_predictions.fields.back().signature.value() =
        kUsernameFieldSignature.value() - 1;

    // Add the username field.
    form_predictions.fields.emplace_back();
    form_predictions.fields.back().renderer_id = kUsernameRendererId;
    form_predictions.fields.back().signature = kUsernameFieldSignature;

    votes_uploader.set_single_username_vote_data(kUsernameRendererId,
                                                 form_predictions);

    ServerFieldTypeSet expected_types = {credentials_saved ? SINGLE_USERNAME
                                                           : NOT_USERNAME};
    EXPECT_CALL(mock_autofill_download_manager_,
                StartUploadRequest(SignatureIs(kFormSignature), false,
                                   expected_types, std::string(), true,
                                   /* pref_service= */ nullptr));

    votes_uploader.MaybeSendSingleUsernameVote(credentials_saved);
  }
}

TEST_F(VotesUploaderTest, SaveSingleUsernameVote) {
  VotesUploader votes_uploader(&client_, false);
  constexpr autofill::FieldRendererId kUsernameRendererId(101);
  constexpr autofill::FieldSignature kUsernameFieldSignature(1234);
  constexpr autofill::FormSignature kFormSignature(1000);

  FormPredictions form_predictions;
  form_predictions.form_signature = kFormSignature;

  // Add the username field.
  form_predictions.fields.emplace_back();
  form_predictions.fields.back().renderer_id = kUsernameRendererId;
  form_predictions.fields.back().signature = kUsernameFieldSignature;

  votes_uploader.set_single_username_vote_data(kUsernameRendererId,
                                               form_predictions);

  // Init store and expect that adding field info is called.
  scoped_refptr<MockPasswordStore> store = new MockPasswordStore;
  store->Init(/*prefs=*/nullptr);

#if defined(OS_ANDROID)
  EXPECT_CALL(*store, AddFieldInfoImpl).Times(0);
#else
  EXPECT_CALL(*store,
              AddFieldInfoImpl(FieldInfoHasData(
                  kFormSignature, kUsernameFieldSignature, SINGLE_USERNAME)));
#endif  // defined(OS_ANDROID)

  // Init FieldInfoManager.
  FieldInfoManagerImpl field_info_manager(store);
  EXPECT_CALL(client_, GetFieldInfoManager())
      .WillRepeatedly(Return(&field_info_manager));

  votes_uploader.MaybeSendSingleUsernameVote(true /*  credentials_saved */);
  task_environment_.RunUntilIdle();
  store->ShutdownOnUIThread();
}

TEST_F(VotesUploaderTest, DontUploadSingleUsernameWhenAlreadyUploaded) {
  VotesUploader votes_uploader(&client_, false);
  constexpr autofill::FieldRendererId kUsernameRendererId(101);
  constexpr autofill::FieldSignature kUsernameFieldSignature(1234);
  constexpr autofill::FormSignature kFormSignature(1000);

  MockFieldInfoManager mock_field_manager;
  ON_CALL(client_, GetFieldInfoManager())
      .WillByDefault(Return(&mock_field_manager));
  // Simulate that the vote has been already uploaded.
  ON_CALL(mock_field_manager,
          GetFieldType(kFormSignature, kUsernameFieldSignature))
      .WillByDefault(Return(SINGLE_USERNAME));

  FormPredictions form_predictions;
  form_predictions.form_signature = kFormSignature;

  // Add the username field.
  form_predictions.fields.emplace_back();
  form_predictions.fields.back().renderer_id = kUsernameRendererId;
  form_predictions.fields.back().signature = kUsernameFieldSignature;

  votes_uploader.set_single_username_vote_data(kUsernameRendererId,
                                               form_predictions);

  // Expect no upload, since the vote has been already uploaded.
  EXPECT_CALL(mock_autofill_download_manager_, StartUploadRequest).Times(0);

  votes_uploader.MaybeSendSingleUsernameVote(true /*credentials_saved*/);
}

}  // namespace password_manager
