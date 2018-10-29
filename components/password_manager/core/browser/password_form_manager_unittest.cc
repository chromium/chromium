// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_form_manager.h"

#include <map>
#include <memory>
#include <set>
#include <utility>

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/metrics_hashes.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/proto/server.pb.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/password_manager/core/browser/fake_form_fetcher.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/statistics_table.h"
#include "components/password_manager/core/browser/stub_credentials_filter.h"
#include "components/password_manager/core/browser/stub_form_saver.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/password_manager/core/browser/vote_uploads_test_matchers.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using autofill::features::kAutofillEnforceMinRequiredFieldsForHeuristics;
using autofill::features::kAutofillEnforceMinRequiredFieldsForQuery;
using autofill::features::kAutofillEnforceMinRequiredFieldsForUpload;
using autofill::FieldPropertiesFlags;
using autofill::FieldPropertiesMask;
using autofill::PasswordForm;
using autofill::ValueElementPair;
using base::ASCIIToUTF16;
using ::testing::_;
using ::testing::AllOf;
using ::testing::AtMost;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::InSequence;
using ::testing::IsEmpty;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Not;
using ::testing::Pair;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::SaveArgPointee;
using ::testing::UnorderedElementsAre;
using ::testing::WithArg;

namespace password_manager {

namespace {

// Enum that describes what button the user pressed on the save prompt.
enum SavePromptInteraction { SAVE, NEVER, NO_INTERACTION };

// Creates a form with 2 text fields and 1 password field. The first and second
// fields are username and password respectively. |observed_form| is used for
// default values.
// TODO(crbug.com/824834): The minimal case should be a smaller form.
PasswordForm CreateMinimalCrowdsourcableForm(
    const PasswordForm& observed_form) {
  PasswordForm form = observed_form;
  form.origin = GURL("https://www.foo.com/login");
  form.form_data.origin = form.origin;
  autofill::FormFieldData field;
  field.name = ASCIIToUTF16("email");
  field.form_control_type = "text";
  form.form_data.fields.push_back(field);
  field.name = ASCIIToUTF16("password");
  field.form_control_type = "password";
  form.form_data.fields.push_back(field);
  field.name = ASCIIToUTF16("petname");
  field.form_control_type = "text";
  form.form_data.fields.push_back(field);
  form.username_element = ASCIIToUTF16("email");
  form.password_element = ASCIIToUTF16("password");
  return form;
}

class MockFormSaver : public StubFormSaver {
 public:
  MockFormSaver() = default;

  ~MockFormSaver() override = default;

  // FormSaver:
  MOCK_METHOD1(PermanentlyBlacklist, void(autofill::PasswordForm* observed));
  MOCK_METHOD2(
      Save,
      void(const autofill::PasswordForm& pending,
           const std::map<base::string16, const PasswordForm*>& best_matches));
  MOCK_METHOD4(
      Update,
      void(const autofill::PasswordForm& pending,
           const std::map<base::string16, const PasswordForm*>& best_matches,
           const std::vector<autofill::PasswordForm>* credentials_to_update,
           const autofill::PasswordForm* old_primary_key));
  MOCK_METHOD1(PresaveGeneratedPassword,
               void(const autofill::PasswordForm& generated));
  MOCK_METHOD0(RemovePresavedPassword, void());

  std::unique_ptr<FormSaver> Clone() override {
    return std::make_unique<MockFormSaver>();
  }

  // Convenience downcasting method.
  static MockFormSaver& Get(PasswordFormManager* form_manager) {
    return *static_cast<MockFormSaver*>(form_manager->form_saver());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockFormSaver);
};

class MockFormFetcher : public FakeFormFetcher {
 public:
  MOCK_METHOD1(AddConsumer, void(Consumer*));
  MOCK_METHOD1(RemoveConsumer, void(Consumer*));
  MOCK_METHOD0(Clone, std::unique_ptr<FormFetcher>());
};

MATCHER_P(UsernamePtrIs, username_value, "") {
  if (!arg)
    return false;
  if (arg->username_value != username_value) {
    *result_listener << "has username: " << arg->username_value;
    return false;
  }
  return true;
}

MATCHER_P(PasswordsWereRevealed, revealed, "") {
  return arg.passwords_were_revealed() == revealed;
}

MATCHER_P(HasPasswordAttributesVote, is_vote_expected, "") {
  base::Optional<std::pair<autofill::PasswordAttribute, bool>> vote =
      arg.get_password_attributes_vote_for_testing();
  EXPECT_EQ(is_vote_expected, vote.has_value());
  if (vote.has_value()) {
    size_t reported_length = arg.get_password_length_vote_for_testing();
    EXPECT_LT(0u, reported_length);
    EXPECT_LE(reported_length, 5u /* actual password length */);
  }
  return true;
}

// Matches iff the masks in |expected_field_properties| match the mask in the
// uploaded form exactly.
MATCHER_P(UploadedFieldPropertiesMasksAre, expected_field_properties, "") {
  size_t matched_count = 0;
  bool conflict_found = false;
  for (const auto& field : arg) {
    auto expectation = expected_field_properties.find(field->name);
    if (expectation == expected_field_properties.end())
      continue;

    matched_count++;
    autofill::FieldPropertiesMask expected_mask = expectation->second;

    if (field->properties_mask != expected_mask) {
      *result_listener << (conflict_found ? ", " : "") << field->name
                       << " field: expected mask " << expected_mask
                       << ", but found " << field->properties_mask;
      conflict_found = true;
    }
  }

  if (matched_count != expected_field_properties.size()) {
    *result_listener
        << (conflict_found ? ", " : "")
        << "some expectations did not correspond to an uploaded field";
    conflict_found = true;
  }

  return !conflict_found;
}

class MockAutofillDownloadManager : public autofill::AutofillDownloadManager {
 public:
  MockAutofillDownloadManager(
      autofill::AutofillDriver* driver,
      autofill::AutofillDownloadManager::Observer* observer)
      : AutofillDownloadManager(driver, observer) {}

  MOCK_METHOD6(StartUploadRequest,
               bool(const autofill::FormStructure&,
                    bool,
                    const autofill::ServerFieldTypeSet&,
                    const std::string&,
                    bool,
                    PrefService*));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAutofillDownloadManager);
};

class MockAutofillManager : public autofill::AutofillManager {
 public:
  MockAutofillManager(autofill::AutofillDriver* driver,
                      autofill::AutofillClient* client,
                      autofill::PersonalDataManager* data_manager)
      : AutofillManager(driver, client, data_manager) {}

  void SetDownloadManager(autofill::AutofillDownloadManager* manager) {
    set_download_manager(manager);
  }

  // Workaround for std::unique_ptr<> lacking a copy constructor.
  bool MaybeStartVoteUploadProcess(
      std::unique_ptr<FormStructure> form_structure,
      const base::TimeTicks& timestamp,
      bool observed_submission) override {
    MaybeStartVoteUploadProcessPtr(form_structure.release(), timestamp,
                                   observed_submission);
    return true;
  }

  MOCK_METHOD3(MaybeStartVoteUploadProcessPtr,
               void(FormStructure*, const base::TimeTicks&, bool));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAutofillManager);
};

class MockPasswordManagerDriver : public StubPasswordManagerDriver {
 public:
  MockPasswordManagerDriver()
      : mock_autofill_manager_(&test_autofill_driver_,
                               &test_autofill_client_,
                               &test_personal_data_manager_) {
    std::unique_ptr<TestingPrefServiceSimple> prefs(
        new TestingPrefServiceSimple());
    prefs->registry()->RegisterBooleanPref(
        autofill::prefs::kAutofillCreditCardEnabled, true);
    prefs->registry()->RegisterBooleanPref(
        autofill::prefs::kAutofillProfileEnabled, true);
    test_autofill_client_.SetPrefs(std::move(prefs));
    mock_autofill_download_manager_ = new MockAutofillDownloadManager(
        &test_autofill_driver_, &mock_autofill_manager_);
    // AutofillManager takes ownership of |mock_autofill_download_manager_|.
    mock_autofill_manager_.SetDownloadManager(mock_autofill_download_manager_);
  }

  ~MockPasswordManagerDriver() override {}

  MOCK_METHOD1(FillPasswordForm, void(const autofill::PasswordFormFillData&));
  MOCK_METHOD0(InformNoSavedCredentials, void());
  MOCK_METHOD1(ShowInitialPasswordAccountSuggestions,
               void(const autofill::PasswordFormFillData&));
  MOCK_METHOD1(AllowPasswordGenerationForForm,
               void(const autofill::PasswordForm&));

  MockAutofillManager* mock_autofill_manager() {
    return &mock_autofill_manager_;
  }

  MockAutofillDownloadManager* mock_autofill_download_manager() {
    return mock_autofill_download_manager_;
  }

 private:
  autofill::TestAutofillDriver test_autofill_driver_;
  autofill::TestAutofillClient test_autofill_client_;
  autofill::TestPersonalDataManager test_personal_data_manager_;
  MockAutofillDownloadManager* mock_autofill_download_manager_;

  NiceMock<MockAutofillManager> mock_autofill_manager_;
};

class TestPasswordManagerClient : public StubPasswordManagerClient {
 public:
  TestPasswordManagerClient()
      : driver_(new NiceMock<MockPasswordManagerDriver>) {
    prefs_ = std::make_unique<TestingPrefServiceSimple>();
    prefs_->registry()->RegisterBooleanPref(prefs::kCredentialsEnableService,
                                            true);
  }

  PrefService* GetPrefs() const override { return prefs_.get(); }

  MockPasswordManagerDriver* mock_driver() { return driver_.get(); }

  base::WeakPtr<PasswordManagerDriver> driver() { return driver_->AsWeakPtr(); }

  autofill::AutofillManager* GetAutofillManagerForMainFrame() override {
    return mock_driver()->mock_autofill_manager();
  }

  void KillDriver() { driver_.reset(); }

  const GURL& GetMainFrameURL() const override {
    static GURL url("https://www.example.com");
    return url;
  }

 private:
  std::unique_ptr<TestingPrefServiceSimple> prefs_;
  std::unique_ptr<MockPasswordManagerDriver> driver_;
};

ACTION_P(SaveToUniquePtr, scoped) {
  scoped->reset(arg0);
}

}  // namespace

class PasswordFormManagerTest : public testing::Test {
 public:
  PasswordFormManagerTest() { fake_form_fetcher_.Fetch(); }

  void SetUp() override {
    observed_form_.origin = GURL("http://accounts.google.com/a/LoginAuth");
    observed_form_.action = GURL("http://accounts.google.com/a/Login");
    observed_form_.username_element = ASCIIToUTF16("Email");
    observed_form_.password_element = ASCIIToUTF16("Passwd");
    observed_form_.submit_element = ASCIIToUTF16("signIn");
    observed_form_.signon_realm = "http://accounts.google.com";
    observed_form_.form_data.name = ASCIIToUTF16("the-form-name");

    saved_match_ = observed_form_;
    saved_match_.origin = GURL("http://accounts.google.com/a/ServiceLoginAuth");
    saved_match_.action = GURL("http://accounts.google.com/a/ServiceLogin");
    saved_match_.preferred = true;
    saved_match_.username_value = ASCIIToUTF16("test@gmail.com");
    saved_match_.password_value = ASCIIToUTF16("test1");
    saved_match_.other_possible_usernames.push_back(ValueElementPair(
        ASCIIToUTF16("test2@gmail.com"), ASCIIToUTF16("full_name")));
    saved_match_.all_possible_passwords = {
        {ASCIIToUTF16("password"), base::string16()},
        {ASCIIToUTF16("password"), ASCIIToUTF16("Passwd")}};

    autofill::FormFieldData field;
    field.label = ASCIIToUTF16("Full name");
    field.name = ASCIIToUTF16("full_name");
    field.form_control_type = "text";
    saved_match_.form_data.fields.push_back(field);

    field.label = ASCIIToUTF16("Email");
    field.name = ASCIIToUTF16("Email");
    field.form_control_type = "text";
    saved_match_.form_data.fields.push_back(field);

    field.label = ASCIIToUTF16("password");
    field.name = ASCIIToUTF16("Passwd");
    field.form_control_type = "password";
    saved_match_.form_data.fields.push_back(field);

    psl_saved_match_ = saved_match_;
    psl_saved_match_.is_public_suffix_match = true;
    psl_saved_match_.origin =
        GURL("http://m.accounts.google.com/a/ServiceLoginAuth");
    psl_saved_match_.action = GURL("http://m.accounts.google.com/a/Login");
    psl_saved_match_.signon_realm = "http://m.accounts.google.com";

    password_manager_.reset(new PasswordManager(&client_));
    form_manager_.reset(new PasswordFormManager(
        password_manager_.get(), &client_, client_.driver(), observed_form_,
        std::make_unique<NiceMock<MockFormSaver>>(), &fake_form_fetcher_));
    form_manager_->Init(nullptr);
  }

  // Save saved_match() for observed_form() where |observed_form_data|,
  // |times_used|, and |status| are used to overwrite the default values for
  // observed_form(). |field_type| is the upload that we expect from saving,
  // with nullptr meaning no upload expected.
  void AccountCreationUploadTest(const autofill::FormData& observed_form_data,
                                 int times_used,
                                 PasswordForm::GenerationUploadStatus status,
                                 const autofill::ServerFieldType* field_type) {
    PasswordForm form(*observed_form());

    form.form_data = observed_form_data;

    FakeFormFetcher fetcher;
    fetcher.Fetch();
    PasswordFormManager form_manager(
        password_manager(), client(), client()->driver(), form,
        std::make_unique<NiceMock<MockFormSaver>>(), &fetcher);
    form_manager.Init(nullptr);
    PasswordForm match = CreateSavedMatch(false);
    match.generation_upload_status = status;
    match.times_used = times_used;

    PasswordForm form_to_save(form);
    form_to_save.preferred = true;
    form_to_save.username_element = ASCIIToUTF16("observed-username-field");
    form_to_save.password_element = ASCIIToUTF16("observed-password-field");
    form_to_save.username_value = match.username_value;
    form_to_save.password_value = match.password_value;

    fetcher.SetNonFederated({&match}, 0u);
    std::string expected_login_signature;
    autofill::FormStructure observed_structure(observed_form_data);
    autofill::FormStructure pending_structure(saved_match()->form_data);
    if (observed_structure.FormSignatureAsStr() !=
            pending_structure.FormSignatureAsStr() &&
        times_used == 0) {
      expected_login_signature = observed_structure.FormSignatureAsStr();
    }
    autofill::ServerFieldTypeSet expected_available_field_types;
    FieldTypeMap expected_types;
    expected_types[ASCIIToUTF16("full_name")] = autofill::UNKNOWN_TYPE;

    // When we're voting for an account creation form, we should also vote
    // for its username field.
    bool expect_username_vote = false;
    if (field_type && *field_type == autofill::ACCOUNT_CREATION_PASSWORD) {
      expected_types[match.username_element] = autofill::USERNAME;
      expected_available_field_types.insert(autofill::USERNAME);
      expect_username_vote = true;
    } else {
      expected_types[match.username_element] = autofill::UNKNOWN_TYPE;
    }

    bool expect_generation_vote = false;
    if (field_type) {
      // Show the password generation popup to check that the generation vote
      // would be ignored.
      form_manager.SetGenerationElement(saved_match()->password_element);
      form_manager.SetGenerationPopupWasShown(/*shown=*/true,
                                              /*manually_triggered*/ true);
      expect_generation_vote =
          *field_type != autofill::ACCOUNT_CREATION_PASSWORD;

      expected_available_field_types.insert(*field_type);
      expected_types[saved_match()->password_element] = *field_type;
    }

    if (field_type) {
      if (expect_username_vote) {
        EXPECT_CALL(
            *client()->mock_driver()->mock_autofill_download_manager(),
            StartUploadRequest(AllOf(SignatureIsSameAs(*saved_match()),
                                     UploadedAutofillTypesAre(expected_types),
                                     HasGenerationVote(expect_generation_vote),
                                     VoteTypesAre(VoteTypeMap(
                                         {{match.username_element,
                                           autofill::AutofillUploadContents::
                                               Field::CREDENTIALS_REUSED}})),
                                     HasPasswordAttributesVote(false)),
                               false, expected_available_field_types,
                               expected_login_signature, true, nullptr));
      } else {
        EXPECT_CALL(
            *client()->mock_driver()->mock_autofill_download_manager(),
            StartUploadRequest(AllOf(SignatureIsSameAs(*saved_match()),
                                     UploadedAutofillTypesAre(expected_types),
                                     HasGenerationVote(expect_generation_vote),
                                     HasPasswordAttributesVote(false)),
                               false, expected_available_field_types,
                               expected_login_signature, true, nullptr));
      }
    } else {
      EXPECT_CALL(*client()->mock_driver()->mock_autofill_download_manager(),
                  StartUploadRequest(_, _, _, _, _, _))
          .Times(0);
    }
    if (times_used == 0) {
      // First login vote.
      EXPECT_CALL(*client()->mock_driver()->mock_autofill_download_manager(),
                  StartUploadRequest(SignatureIsSameAs(form_to_save), _, _, _,
                                     _, nullptr));
    }
    form_manager.ProvisionallySave(form_to_save);
    form_manager.Save();
    Mock::VerifyAndClearExpectations(
        client()->mock_driver()->mock_autofill_download_manager());
  }

  // Test upload votes on change password forms. |field_type| is a vote that we
  // expect to be uploaded.
  void ChangePasswordUploadTest(autofill::ServerFieldType field_type,
                                bool has_confirmation_field) {
    SCOPED_TRACE(testing::Message()
                 << "field_type=" << field_type
                 << " has_confirmation_field=" << has_confirmation_field);

    // |observed_form_| should have |form_data| in order to be uploaded.
    observed_form()->form_data = saved_match()->form_data;
    // Turn |observed_form_| and  into change password form.
    observed_form()->new_password_element = ASCIIToUTF16("NewPasswd");
    autofill::FormFieldData field;
    field.label = ASCIIToUTF16("NewPasswd");
    field.name = ASCIIToUTF16("NewPasswd");
    field.form_control_type = "password";
    observed_form()->form_data.fields.push_back(field);
    autofill::FormFieldData empty_field;
    observed_form()->form_data.fields.push_back(empty_field);
    if (has_confirmation_field) {
      field.label = ASCIIToUTF16("ConfPwd");
      field.name = ASCIIToUTF16("ConfPwd");
      field.form_control_type = "password";
      observed_form()->form_data.fields.push_back(field);
    }

    FakeFormFetcher fetcher;
    fetcher.Fetch();
    PasswordFormManager form_manager(
        password_manager(), client(), client()->driver(), *observed_form(),
        std::make_unique<NiceMock<MockFormSaver>>(), &fetcher);
    form_manager.Init(nullptr);
    fetcher.SetNonFederated({saved_match()}, 0u);

    // User submits current and new credentials to the observed form.
    PasswordForm submitted_form(*observed_form());
    // credentials.username_element.clear();
    submitted_form.username_value = saved_match()->username_value;
    submitted_form.password_value = saved_match()->password_value;
    submitted_form.new_password_value = ASCIIToUTF16("test2");
    if (has_confirmation_field)
      submitted_form.confirmation_password_element = ASCIIToUTF16("ConfPwd");
    submitted_form.preferred = true;
    form_manager.ProvisionallySave(submitted_form);

    // Successful login. The PasswordManager would instruct PasswordFormManager
    // to update.
    EXPECT_FALSE(form_manager.IsNewLogin());
    EXPECT_FALSE(form_manager.IsPossibleChangePasswordFormWithoutUsername());

    // By now, the PasswordFormManager should have promoted the new password
    // value already to be the current password, and should no longer maintain
    // any info about the new password value.
    EXPECT_EQ(submitted_form.new_password_value,
              form_manager.GetPendingCredentials().password_value);
    EXPECT_TRUE(
        form_manager.GetPendingCredentials().new_password_value.empty());

    std::map<base::string16, autofill::ServerFieldType> expected_types;
    expected_types[ASCIIToUTF16("full_name")] = autofill::UNKNOWN_TYPE;
    expected_types[observed_form_.username_element] = autofill::UNKNOWN_TYPE;
    expected_types[observed_form_.password_element] = autofill::PASSWORD;
    expected_types[observed_form_.new_password_element] = field_type;
    expected_types[base::string16()] = autofill::UNKNOWN_TYPE;

    autofill::ServerFieldTypeSet expected_available_field_types;
    expected_available_field_types.insert(autofill::PASSWORD);
    expected_available_field_types.insert(field_type);
    if (has_confirmation_field) {
      expected_types[submitted_form.confirmation_password_element] =
          autofill::CONFIRMATION_PASSWORD;
      expected_available_field_types.insert(autofill::CONFIRMATION_PASSWORD);
    }

    std::string expected_login_signature;
    if (field_type == autofill::NEW_PASSWORD) {
      autofill::FormStructure pending_structure(saved_match()->form_data);
      expected_login_signature = pending_structure.FormSignatureAsStr();

      // An unrelated vote that the credentials were used for the first time.
      EXPECT_CALL(
          *client()->mock_driver()->mock_autofill_download_manager(),
          StartUploadRequest(SignatureIsSameAs(submitted_form), _, _, _, _, _));
    }
    EXPECT_CALL(
        *client()->mock_driver()->mock_autofill_download_manager(),
        StartUploadRequest(
            AllOf(SignatureIsSameAs(*observed_form()),
                  UploadedAutofillTypesAre(expected_types),
                  HasGenerationVote(false), HasPasswordAttributesVote(false)),
            false, expected_available_field_types, expected_login_signature,
            true, nullptr));

    switch (field_type) {
      case autofill::NEW_PASSWORD:
        form_manager.Update(*saved_match());
        break;
      case autofill::PROBABLY_NEW_PASSWORD:
        form_manager.OnNoInteraction(true /* it is an update */);
        break;
      case autofill::NOT_NEW_PASSWORD:
        form_manager.OnNopeUpdateClicked();
        break;
      default:
        NOTREACHED();
    }
    Mock::VerifyAndClearExpectations(
        client()->mock_driver()->mock_autofill_download_manager());
  }

  autofill::AutofillUploadContents::Field::PasswordGenerationType
  GetExpectedPasswordGenerationType(bool is_manual_generation,
                                    bool is_change_password_form,
                                    bool has_generated_password) {
    if (!has_generated_password)
      return autofill::AutofillUploadContents::Field::IGNORED_GENERATION_POPUP;

    if (is_manual_generation) {
      if (is_change_password_form) {
        return autofill::AutofillUploadContents::Field::
            MANUALLY_TRIGGERED_GENERATION_ON_CHANGE_PASSWORD_FORM;
      } else {
        return autofill::AutofillUploadContents::Field::
            MANUALLY_TRIGGERED_GENERATION_ON_SIGN_UP_FORM;
      }
    } else {
      if (is_change_password_form) {
        return autofill::AutofillUploadContents::Field::
            AUTOMATICALLY_TRIGGERED_GENERATION_ON_CHANGE_PASSWORD_FORM;
      } else {
        return autofill::AutofillUploadContents::Field::
            AUTOMATICALLY_TRIGGERED_GENERATION_ON_SIGN_UP_FORM;
      }
    }
  }

  // The user types username and generates password on SignUp or change password
  // form. The password generation might be triggered automatically or manually.
  // This function checks that correct vote is uploaded on server. The vote must
  // be uploaded regardless of the user's interaction with the prompt.
  void GeneratedVoteUploadTest(bool is_manual_generation,
                               bool is_change_password_form,
                               bool has_generated_password,
                               bool generated_password_changed,
                               SavePromptInteraction interaction) {
    SCOPED_TRACE(testing::Message()
                 << "is_manual_generation=" << is_manual_generation
                 << " is_change_password_form=" << is_change_password_form
                 << " has_generated_password=" << has_generated_password
                 << " generated_password_changed=" << generated_password_changed
                 << " interaction=" << interaction);
    PasswordForm form(*observed_form());
    form.form_data = saved_match()->form_data;

    if (is_change_password_form) {
      // Turn |form| to a change password form.
      form.new_password_element = ASCIIToUTF16("NewPasswd");

      autofill::FormFieldData field;
      field.label = ASCIIToUTF16("password");
      field.name = ASCIIToUTF16("NewPasswd");
      field.form_control_type = "password";
      form.form_data.fields.push_back(field);
    }

    // Create submitted form.
    PasswordForm submitted_form(form);
    submitted_form.preferred = true;
    submitted_form.username_value = saved_match()->username_value;
    submitted_form.password_value = saved_match()->password_value;

    if (is_change_password_form) {
      submitted_form.new_password_value =
          saved_match()->password_value + ASCIIToUTF16("1");
      submitted_form.password_value = submitted_form.new_password_value;
    }

    FakeFormFetcher fetcher;
    fetcher.Fetch();
    PasswordFormManager form_manager(
        password_manager(), client(), client()->driver(), form,
        std::make_unique<NiceMock<MockFormSaver>>(), &fetcher);
    form_manager.Init(nullptr);
    fetcher.SetNonFederated(std::vector<const PasswordForm*>(), 0u);

    autofill::ServerFieldTypeSet expected_available_field_types;
    // Don't send autofill votes if the user didn't press "Save" button.
    if (interaction == SAVE)
      expected_available_field_types.insert(autofill::PASSWORD);

    base::string16 generation_element = is_change_password_form
                                            ? form.new_password_element
                                            : form.password_element;
    form_manager.SetGenerationElement(generation_element);
    form_manager.SetGenerationPopupWasShown(true, is_manual_generation);
    if (has_generated_password) {
      form_manager.PresaveGeneratedPassword(submitted_form);
      if (generated_password_changed) {
        submitted_form.password_value += ASCIIToUTF16("2");
        form_manager.PresaveGeneratedPassword(submitted_form);
      }
    }

    // Figure out expected generation event type.
    autofill::AutofillUploadContents::Field::PasswordGenerationType
        expected_generation_type = GetExpectedPasswordGenerationType(
            is_manual_generation, is_change_password_form,
            has_generated_password);
    std::map<base::string16,
             autofill::AutofillUploadContents::Field::PasswordGenerationType>
        expected_generation_types;
    expected_generation_types[generation_element] = expected_generation_type;

    EXPECT_CALL(*client()->mock_driver()->mock_autofill_download_manager(),
                StartUploadRequest(AllOf(SignatureIsSameAs(submitted_form),
                                         UploadedGenerationTypesAre(
                                             expected_generation_types,
                                             generated_password_changed)),
                                   false, expected_available_field_types,
                                   std::string(), true, nullptr));
    base::HistogramTester histogram_tester;

    form_manager.ProvisionallySave(submitted_form);
    switch (interaction) {
      case SAVE:
        form_manager.Save();
        break;
      case NEVER:
        form_manager.OnNeverClicked();
        break;
      case NO_INTERACTION:
        form_manager.OnNoInteraction(false /* not an update prompt*/);
        break;
    }
    if (has_generated_password) {
      histogram_tester.ExpectUniqueSample(
          "PasswordGeneration.GeneratedPasswordWasEdited",
          generated_password_changed /* sample */, 1);
      histogram_tester.ExpectUniqueSample(
          "PasswordGeneration.IsTriggeredManually",
          is_manual_generation /* sample */, 1);
    }
    Mock::VerifyAndClearExpectations(
        client()->mock_driver()->mock_autofill_download_manager());
  }

  void GeneratedPasswordUkmTest(bool is_manual_generation,
                                bool is_change_password_form,
                                bool has_generated_password,
                                bool generated_password_changed,
                                SavePromptInteraction interaction) {
    SCOPED_TRACE(testing::Message()
                 << "is_manual_generation=" << is_manual_generation
                 << " is_change_password_form=" << is_change_password_form
                 << " has_generated_password=" << has_generated_password
                 << " generated_password_changed=" << generated_password_changed
                 << " interaction=" << interaction);

    ukm::TestAutoSetUkmRecorder test_ukm_recorder;
    test_ukm_recorder.UpdateSourceURL(client()->GetUkmSourceId(),
                                      client()->GetMainFrameURL());

    PasswordForm form(*observed_form());
    form.form_data = saved_match()->form_data;

    if (is_change_password_form) {
      // Turn |form| to a change password form.
      form.new_password_element = ASCIIToUTF16("NewPasswd");

      autofill::FormFieldData field;
      field.label = ASCIIToUTF16("password");
      field.name = ASCIIToUTF16("NewPasswd");
      field.form_control_type = "password";
      form.form_data.fields.push_back(field);
    }

    // Create submitted form.
    PasswordForm submitted_form(form);
    submitted_form.preferred = true;
    submitted_form.username_value = saved_match()->username_value;
    submitted_form.password_value = saved_match()->password_value;

    if (is_change_password_form) {
      submitted_form.new_password_value =
          saved_match()->password_value + ASCIIToUTF16("1");
      submitted_form.password_value = submitted_form.new_password_value;
    }

    FakeFormFetcher fetcher;
    fetcher.Fetch();
    auto metrics_recorder = base::MakeRefCounted<PasswordFormMetricsRecorder>(
        form.origin.SchemeIsCryptographic(), client()->GetUkmSourceId());
    auto form_manager = std::make_unique<PasswordFormManager>(
        password_manager(), client(), client()->driver(), form,
        std::make_unique<NiceMock<MockFormSaver>>(), &fetcher);
    // *Move* the metrics recorder to not hold on to a reference.
    form_manager->Init(std::move(metrics_recorder));
    fetcher.SetNonFederated(std::vector<const PasswordForm*>(), 0u);

    autofill::ServerFieldTypeSet expected_available_field_types;
    // Don't send autofill votes if the user didn't press "Save" button.
    if (interaction == SAVE)
      expected_available_field_types.insert(autofill::PASSWORD);

    base::string16 generation_element = is_change_password_form
                                            ? form.new_password_element
                                            : form.password_element;
    form_manager->SetGenerationElement(generation_element);
    form_manager->SetGenerationPopupWasShown(true, is_manual_generation);
    if (has_generated_password) {
      form_manager->PresaveGeneratedPassword(submitted_form);
      if (generated_password_changed) {
        submitted_form.password_value += ASCIIToUTF16("2");
        form_manager->PresaveGeneratedPassword(submitted_form);
      }
    }

    // Figure out expected generation event type.
    autofill::AutofillUploadContents::Field::PasswordGenerationType
        expected_generation_type = GetExpectedPasswordGenerationType(
            is_manual_generation, is_change_password_form,
            has_generated_password);
    std::map<base::string16,
             autofill::AutofillUploadContents::Field::PasswordGenerationType>
        expected_generation_types;
    expected_generation_types[generation_element] = expected_generation_type;

    EXPECT_CALL(*client()->mock_driver()->mock_autofill_download_manager(),
                StartUploadRequest(AllOf(SignatureIsSameAs(submitted_form),
                                         UploadedGenerationTypesAre(
                                             expected_generation_types,
                                             generated_password_changed)),
                                   false, expected_available_field_types,
                                   std::string(), true, nullptr));
    form_manager->ProvisionallySave(submitted_form);
    switch (interaction) {
      case SAVE:
        form_manager->Save();
        break;
      case NEVER:
        form_manager->OnNeverClicked();
        break;
      case NO_INTERACTION:
        form_manager->OnNoInteraction(false /* not an update prompt*/);
        break;
    }
    // Reset form manager to flush UKM metrics.
    form_manager.reset();

    auto entries = test_ukm_recorder.GetEntriesByName(
        ukm::builders::PasswordForm::kEntryName);
    ASSERT_EQ(1u, entries.size());

    ukm::TestUkmRecorder::ExpectEntryMetric(
        entries[0], ukm::builders::PasswordForm::kGeneration_PopupShownName,
        is_manual_generation
            ? static_cast<int64_t>(
                  password_manager::PasswordFormMetricsRecorder::
                      PasswordGenerationPopupShown::kShownManually)
            : static_cast<int64_t>(
                  password_manager::PasswordFormMetricsRecorder::
                      PasswordGenerationPopupShown::kShownAutomatically));

    ukm::TestUkmRecorder::ExpectEntryMetric(
        entries[0],
        ukm::builders::PasswordForm::kGeneration_GeneratedPasswordName,
        has_generated_password ? 1 : 0);

    if (has_generated_password) {
      ukm::TestUkmRecorder::ExpectEntryMetric(
          entries[0],
          ukm::builders::PasswordForm::
              kGeneration_GeneratedPasswordModifiedName,
          generated_password_changed ? 1 : 0);
    }
    Mock::VerifyAndClearExpectations(
        client()->mock_driver()->mock_autofill_download_manager());
  }

  PasswordForm* observed_form() { return &observed_form_; }
  PasswordForm* saved_match() { return &saved_match_; }
  PasswordForm* psl_saved_match() { return &psl_saved_match_; }
  PasswordForm CreateSavedMatch(bool blacklisted) {
    PasswordForm match = saved_match_;
    match.blacklisted_by_user = blacklisted;
    return match;
  }

  TestPasswordManagerClient* client() { return &client_; }

  PasswordManager* password_manager() { return password_manager_.get(); }

  PasswordFormManager* form_manager() { return form_manager_.get(); }

  FakeFormFetcher* fake_form_fetcher() { return &fake_form_fetcher_; }

  // To spare typing for PasswordFormManager instances which need no driver.
  const base::WeakPtr<PasswordManagerDriver> kNoDriver;

 protected:
  enum class SimulatedManagerAction { NONE, AUTOFILLED, OFFERED, OFFERED_PSL };
  enum class SimulatedSubmitResult { NONE, PASSED, FAILED };
  enum class SuppressedFormType { HTTPS, PSL_MATCH, SAME_ORGANIZATION_NAME };

  PasswordForm CreateSuppressedForm(SuppressedFormType suppression_type,
                                    const char* username,
                                    const char* password,
                                    PasswordForm::Type manual_or_generated) {
    PasswordForm form = *saved_match();
    switch (suppression_type) {
      case SuppressedFormType::HTTPS:
        form.origin = GURL("https://accounts.google.com/a/LoginAuth");
        form.signon_realm = "https://accounts.google.com/";
        break;
      case SuppressedFormType::PSL_MATCH:
        form.origin = GURL("http://other.google.com/");
        form.signon_realm = "http://other.google.com/";
        break;
      case SuppressedFormType::SAME_ORGANIZATION_NAME:
        form.origin = GURL("https://may-or-may-not-be.google.appspot.com/");
        form.signon_realm = "https://may-or-may-not-be.google.appspot.com/";
        break;
    }
    form.type = manual_or_generated;
    form.username_value = ASCIIToUTF16(username);
    form.password_value = ASCIIToUTF16(password);
    return form;
  }

  void SimulateActionsOnHTTPObservedForm(
      FakeFormFetcher* fetcher,
      SimulatedManagerAction manager_action,
      SimulatedSubmitResult submit_result,
      const char* filled_username,
      const char* filled_password,
      const char* submitted_password = nullptr) {
    PasswordFormManager form_manager(
        password_manager(), client(), client()->driver(), *observed_form(),
        std::make_unique<NiceMock<MockFormSaver>>(), fetcher);
    form_manager.Init(nullptr);

    EXPECT_CALL(*client()->mock_driver()->mock_autofill_download_manager(),
                StartUploadRequest(_, _, _, _, _, _))
        .Times(::testing::AnyNumber());

    PasswordForm http_stored_form = *saved_match();
    http_stored_form.username_value = base::ASCIIToUTF16(filled_username);
    http_stored_form.password_value = base::ASCIIToUTF16(filled_password);
    if (manager_action == SimulatedManagerAction::OFFERED_PSL)
      http_stored_form.is_public_suffix_match = true;

    std::vector<const PasswordForm*> matches;
    if (manager_action != SimulatedManagerAction::NONE)
      matches.push_back(&http_stored_form);

    // Extra mile: kChoose is only recorded if there were multiple
    // logins available and the preferred one was changed.
    PasswordForm http_stored_form2 = http_stored_form;
    if (manager_action == SimulatedManagerAction::OFFERED) {
      http_stored_form.preferred = false;
      http_stored_form2.username_value = ASCIIToUTF16("user-other@gmail.com");
      matches.push_back(&http_stored_form2);
    }

    fetcher->Fetch();
    fetcher->SetNonFederated(matches, 0u);

    if (submit_result != SimulatedSubmitResult::NONE) {
      PasswordForm submitted_form(*observed_form());
      submitted_form.preferred = true;
      submitted_form.username_value = base::ASCIIToUTF16(filled_username);
      submitted_form.password_value =
          submitted_password ? base::ASCIIToUTF16(submitted_password)
                             : base::ASCIIToUTF16(filled_password);

      form_manager.ProvisionallySave(submitted_form);
      if (submit_result == SimulatedSubmitResult::PASSED) {
        form_manager.LogSubmitPassed();
        form_manager.Save();
      } else {
        form_manager.LogSubmitFailed();
      }
    }
  }

 private:
  // Necessary for callbacks, and for TestAutofillDriver.
  base::MessageLoop message_loop_;

  PasswordForm observed_form_;
  PasswordForm saved_match_;
  PasswordForm psl_saved_match_;
  TestPasswordManagerClient client_;
  std::unique_ptr<PasswordManager> password_manager_;
  // Define |fake_form_fetcher_| before |form_manager_|, because the former
  // needs to outlive the latter.
  FakeFormFetcher fake_form_fetcher_;
  std::unique_ptr<PasswordFormManager> form_manager_;
};

class PasswordFormManagerFillOnAccountSelectTest
    : public PasswordFormManagerTest {
 public:
  PasswordFormManagerFillOnAccountSelectTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kFillOnAccountSelect);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test provisionally saving a new login.
TEST_F(PasswordFormManagerTest, TestNewLogin) {
  fake_form_fetcher()->SetNonFederated(std::vector<const PasswordForm*>(), 0u);

  // User submits credentials for the observed form.
  PasswordForm credentials = *observed_form();
  credentials.username_value = saved_match()->username_value;
  credentials.password_value = saved_match()->password_value;
  credentials.preferred = true;
  form_manager()->ProvisionallySave(credentials);

  // Successful login. The PasswordManager would instruct PasswordFormManager
  // to save, which should know this is a new login.
  EXPECT_TRUE(form_manager()->IsNewLogin());
  // Make sure the credentials that would be submitted on successful login
  // are going to match the stored entry in the db.
  EXPECT_EQ(observed_form()->origin.spec(),
            form_manager()->GetPendingCredentials().origin.spec());
  EXPECT_EQ(observed_form()->signon_realm,
            form_manager()->GetPendingCredentials().signon_realm);
  EXPECT_EQ(observed_form()->action,
            form_manager()->GetPendingCredentials().action);
  EXPECT_TRUE(form_manager()->GetPendingCredentials().preferred);
  EXPECT_EQ(saved_match()->password_value,
            form_manager()->GetPendingCredentials().password_value);
  EXPECT_EQ(saved_match()->username_value,
            form_manager()->GetPendingCredentials().username_value);
  EXPECT_TRUE(
      form_manager()->GetPendingCredentials().new_password_element.empty());
  EXPECT_TRUE(
      form_manager()->GetPendingCredentials().new_password_value.empty());
}

// Test provisionally saving a new login in presence of other saved logins.
TEST_F(PasswordFormManagerTest, TestAdditionalLogin) {
  fake_form_fetcher()->SetNonFederated({saved_match()}, 0u);

  base::string16 new_user = ASCIIToUTF16("newuser");
  base::string16 new_pass = ASCIIToUTF16("newpass");
  ASSERT_NE(new_user, saved_match()->username_value);

  PasswordForm new_login = *observed_form();
  new_login.username_value = new_user;
  new_login.password_value = new_pass;
  new_login.preferred = true;

  form_manager()->ProvisionallySave(new_login);

  // The username value differs from the saved match, so this is a new login.
  EXPECT_TRUE(form_manager()->IsNewLogin());

  EXPECT_EQ(observed_form()->origin.spec(),
            form_manager()->GetPendingCredentials().origin.spec());
  EXPECT_EQ(observed_form()->signon_realm,
            form_manager()->GetPendingCredentials().signon_realm);
  EXPECT_TRUE(form_manager()->GetPendingCredentials().preferred);
  EXPECT_EQ(new_pass, form_manager()->GetPendingCredentials().password_value);
  EXPECT_EQ(new_user, form_manager()->GetPendingCredentials().username_value);
  EXPECT_TRUE(
      form_manager()->GetPendingCredentials().new_password_element.empty());
  EXPECT_TRUE(
      form_manager()->GetPendingCredentials().new_password_value.empty());
}

// Test blacklisting in the presence of saved results.
TEST_F(PasswordFormManagerTest, TestBlacklist) {
  saved_match()->origin = observed_form()->origin;
  saved_match()->action = observed_form()->action;
  fake_form_fetcher()->SetNonFederated({saved_match()}, 0u);

  PasswordForm new_login = *observed_form();
  new_login.username_value = ASCIIToUTF16("newuser");
  new_login.password_value = ASCIIToUTF16("newpass");
  // Pretend Chrome detected a form submission with |new_login|.
  form_manager()->ProvisionallySave(new_login);

  EXPECT_TRUE(form_manager()->IsNewLogin());
  EXPECT_EQ(observed_form()->origin.spec(),
            form_manager()->GetPendingCredentials().origin.spec());
  EXPECT_EQ(observed_form()->signon_realm,
            form_manager()->GetPendingCredentials().signon_realm);

  const PasswordForm pending_form = form_manager()->GetPendingCredentials();
  PasswordForm actual_add_form;
  // Now pretend the user wants to never save passwords on this origin. Chrome
  // is supposed to only request blacklisting of a single form.
  EXPECT_CALL(MockFormSaver::Get(form_manager()), PermanentlyBlacklist(_))
      .WillOnce(SaveArgPointee<0>(&actual_add_form));
  form_manager()->PermanentlyBlacklist();
  EXPECT_EQ(pending_form, form_manager()->GetPendingCredentials());
  // The PasswordFormManager should have updated its knowledge of blacklisting
  // without waiting for PasswordStore updates.
  EXPECT_TRUE(form_manager()->IsBlacklisted());
  EXPECT_THAT(form_manager()->GetBlacklistedMatches(),
              UnorderedElementsAre(Pointee(actual_add_form)));
}

// Test that stored blacklisted forms are correctly evaluated for whether they
// apply to the observed form.
TEST_F(PasswordFormManagerTest, TestBlacklistMatching) {
  // Doesn't apply because it is just a PSL match of the observed form.
  PasswordForm blacklisted_psl = *observed_form();
  blacklisted_psl.signon_realm = "http://m.accounts.google.com";
  blacklisted_psl.is_public_suffix_match = true;
  blacklisted_psl.blacklisted_by_user = true;

  // Doesn't apply because of different PasswordForm::Scheme.
  PasswordForm blacklisted_not_match = *observed_form();
  blacklisted_not_match.scheme = PasswordForm::SCHEME_BASIC;

  // Applies despite different element names and path.
  PasswordForm blacklisted_match = *observed_form();
  blacklisted_match.origin = GURL("http://accounts.google.com/a/LoginAuth1234");
  blacklisted_match.username_element = ASCIIToUTF16("Element1");
  blacklisted_match.password_element = ASCIIToUTF16("Element2");
  blacklisted_match.submit_element = ASCIIToUTF16("Element3");
  blacklisted_match.blacklisted_by_user = true;

  std::vector<const PasswordForm*> matches = {&blacklisted_psl,
                                              &blacklisted_not_match,
                                              &blacklisted_match,
                                              saved_match()};
  fake_form_fetcher()->SetNonFederated(matches, 0u);

  EXPECT_TRUE(form_manager()->IsBlacklisted());
  EXPECT_THAT(form_manager()->GetBlacklistedMatches(),
              ElementsAre(Pointee(blacklisted_match)));
  EXPECT_EQ(1u, form_manager()->GetBestMatches().size());
  EXPECT_EQ(*saved_match(), *form_manager()->GetPreferredMatch());
}

// Test that even in the presence of blacklisted matches, the non-blacklisted
// ones are still autofilled.
TEST_F(PasswordFormManagerTest, AutofillBlacklisted) {
  PasswordForm saved_form = *observed_form();
  saved_form.username_value = ASCIIToUTF16("user");
  saved_form.password_value = ASCIIToUTF16("pass");

  PasswordForm blacklisted = *observed_form();
  blacklisted.blacklisted_by_user = true;
  blacklisted.username_value.clear();

  autofill::PasswordFormFillData fill_data;
  EXPECT_CALL(*client()->mock_driver(), FillPasswordForm(_))
      .WillOnce(SaveArg<0>(&fill_data));

  fake_form_fetcher()->SetNonFederated({&saved_form, &blacklisted}, 0u);
  EXPECT_EQ(1u, form_manager()->GetBlacklistedMatches().size());
  EXPECT_TRUE(form_manager()->IsBlacklisted());
  EXPECT_EQ(1u, form_manager()->GetBestMatches().size());
  EXPECT_TRUE(fill_data.additional_logins.empty());
}

// If PSL-matched credentials had been suggested, but the user has overwritten
// the password, the provisionally saved credentials should no longer be
// considered as PSL-matched, so that the exception for not prompting before
// saving PSL-matched credentials should no longer apply.
TEST_F(PasswordFormManagerTest,
       OverriddenPSLMatchedCredentialsNotMarkedAsPSLMatched) {
  // The suggestion needs to be PSL-matched.
  fake_form_fetcher()->SetNonFederated({psl_saved_match()}, 0u);

  // User modifies the suggested password and submits the form.
  PasswordForm credentials(*observed_form());
  credentials.username_value = saved_match()->username_value;
  credentials.password_value =
      saved_match()->password_value + ASCIIToUTF16("modify");
  form_manager()->ProvisionallySave(credentials);

  EXPECT_TRUE(form_manager()->IsNewLogin());
  EXPECT_FALSE(form_manager()->IsPendingCredentialsPublicSuffixMatch());
}

// Test that if a PSL-matched suggestion is saved on a new origin, its metadata
// are correctly updated.
TEST_F(PasswordFormManagerTest, PSLMatchedCredentialsMetadataUpdated) {
  PasswordForm psl_suggestion = *saved_match();
  psl_suggestion.is_public_suffix_match = true;
  fake_form_fetcher()->SetNonFederated({&psl_suggestion}, 0u);

  PasswordForm submitted_form(*observed_form());
  submitted_form.preferred = true;
  submitted_form.username_value = saved_match()->username_value;
  submitted_form.password_value = saved_match()->password_value;
  form_manager()->ProvisionallySave(submitted_form);

  PasswordForm expected_saved_form(submitted_form);
  expected_saved_form.times_used = 1;
  expected_saved_form.other_possible_usernames.clear();
  expected_saved_form.form_data = saved_match()->form_data;
  expected_saved_form.origin = observed_form()->origin;
  expected_saved_form.is_public_suffix_match = true;
  PasswordForm actual_saved_form;

  autofill::ServerFieldTypeSet expected_available_field_types;
  expected_available_field_types.insert(autofill::ACCOUNT_CREATION_PASSWORD);
  expected_available_field_types.insert(autofill::USERNAME);
  EXPECT_CALL(*client()->mock_driver()->mock_autofill_download_manager(),
              StartUploadRequest(_, false, expected_available_field_types, _,
                                 true, nullptr));
  EXPECT_CALL(MockFormSaver::Get(form_manager()), Save(_, _))
      .WillOnce(SaveArg<0>(&actual_saved_form));
  form_manager()->Save();

  // Can't verify time, so ignore it.
  actual_saved_form.date_created = base::Time();
  EXPECT_EQ(expected_saved_form, actual_saved_form);
}

// Test that when the submitted form contains a "new-password" field, then the
// password value is taken from there.
TEST_F(PasswordFormManagerTest, TestNewLoginFromNewPasswordElement) {
  observed_form()->new_password_element = ASCIIToUTF16("NewPasswd");
  observed_form()->username_marked_by_site = true;

  FakeFormFetcher fetcher;
  fetcher.Fetch();
  PasswordFormManager form_manager(password_manager(), client(),
                                   client()->driver(), *observed_form(),
                                   std::make_unique<MockFormSaver>(), &fetcher);
  form_manager.Init(nullptr);
  fetcher.SetNonFederated(std::vector<const PasswordForm*>(), 0u);

  // User enters current and new credentials to the observed form.
  PasswordForm credentials(*observed_form());
  credentials.username_value = saved_match()->username_value;
  credentials.password_value = ASCIIToUTF16("oldpassword");
  credentials.new_password_value = ASCIIToUTF16("newpassword");
  credentials.preferred = true;
  form_manager.ProvisionallySave(credentials);

  // Successful login. The PasswordManager would instruct PasswordFormManager
  // to save, which should know this is a new login.
  EXPECT_TRUE(form_manager.IsNewLogin());
  EXPECT_EQ(credentials.origin, form_manager.GetPendingCredentials().origin);
  EXPECT_EQ(credentials.signon_realm,
            form_manager.GetPendingCredentials().signon_realm);
  EXPECT_EQ(credentials.action, form_manager.GetPendingCredentials().action);
  EXPECT_TRUE(form_manager.GetPendingCredentials().preferred);
  EXPECT_EQ(credentials.username_value,
            form_manager.GetPendingCredentials().username_value);

  // By this point, the PasswordFormManager should have promoted the new
  // password value to be the current password.
  EXPECT_EQ(credentials.new_password_value,
            form_manager.GetPendingCredentials().password_value);
  EXPECT_EQ(credentials.new_password_element,
            form_manager.GetPendingCredentials().password_element);
  EXPECT_TRUE(form_manager.GetPendingCredentials().new_password_value.empty());
  EXPECT_TRUE(
      form_manager.GetPendingCredentials().new_password_element.empty());
}

TEST_F(PasswordFormManagerTest, TestUpdatePassword) {
  fake_form_fetcher()->SetNonFederated({saved_match()}, 0u);

  // User submits credentials for the observed form using a username previously
  // stored, but a new password. Note that the observed form may have different
  // origin URL (as it does in this case) than the saved_match, but we want to
  // make sure the updated password is reflected in saved_match, because that is
  // what we autofilled.
  base::string16 new_pass = ASCIIToUTF16("test2");
  PasswordForm credentials = *observed_form();
  credentials.username_value = saved_match()->username_value;
  credentials.password_value = new_pass;
  credentials.preferred = true;
  form_manager()->ProvisionallySave(credentials);

  // Successful login. The PasswordManager would instruct PasswordFormManager
  // to save, and since this is an update, it should know not to save as a new
  // login.
  EXPECT_FALSE(form_manager()->IsNewLogin());

  // Make sure the credentials that would be submitted on successful login
  // are going to match the stored entry in the db. (This verifies correct
  // behaviour for bug 1074420).
  EXPECT_EQ(form_manager()->GetPendingCredentials().origin.spec(),
            saved_match()->origin.spec());
  EXPECT_EQ(form_manager()->GetPendingCredentials().signon_realm,
            saved_match()->signon_realm);
  EXPECT_TRUE(form_manager()->GetPendingCredentials().preferred);
  EXPECT_EQ(new_pass, form_manager()->GetPendingCredentials().password_value);
}

TEST_F(PasswordFormManagerTest, TestUpdatePasswordFromNewPasswordElement) {
  // Add a new password field to the test form. The PasswordFormManager should
  // save the password from this field, instead of the current password field.
  observed_form()->new_password_element = ASCIIToUTF16("NewPasswd");

  FakeFormFetcher fetcher;
  fetcher.Fetch();
  PasswordFormManager form_manager(password_manager(), client(),
                                   client()->driver(), *observed_form(),
                                   std::make_unique<MockFormSaver>(), &fetcher);
  form_manager.Init(nullptr);
  fetcher.SetNonFederated({saved_match()}, 0u);

  // User submits current and new credentials to the observed form.
  PasswordForm credentials(*observed_form());
  credentials.username_value = saved_match()->username_value;
  credentials.password_value = saved_match()->password_value;
  credentials.new_password_value = ASCIIToUTF16("test2");
  credentials.preferred = true;
  form_manager.ProvisionallySave(credentials);

  // Successful login. The PasswordManager would instruct PasswordFormManager
  // to save, and since this is an update, it should know not to save as a new
  // login.
  EXPECT_FALSE(form_manager.IsNewLogin());

  // By now, the PasswordFormManager should have promoted the new password value
  // already to be the current password, and should no longer maintain any info
  // about the new password.
  EXPECT_EQ(credentials.new_password_value,
            form_manager.GetPendingCredentials().password_value);
  EXPECT_TRUE(
      form_manager.GetPendingCredentials().new_password_element.empty());
  EXPECT_TRUE(form_manager.GetPendingCredentials().new_password_value.empty());

  // Trigger saving to exercise some special case handling for updating.
  PasswordForm new_credentials;
  EXPECT_CALL(MockFormSaver::Get(&form_manager), Update(_, _, _, nullptr))
      .WillOnce(testing::SaveArg<0>(&new_credentials));

  form_manager.Save();

  // The password should be updated.
  EXPECT_EQ(credentials.new_password_value, new_credentials.password_value);
  EXPECT_EQ(saved_match()->username_element, new_credentials.username_element);
  EXPECT_EQ(saved_match()->password_element, new_credentials.password_element);
  EXPECT_EQ(saved_match()->submit_element, new_credentials.submit_element);
}

// Test that saved results are not ignored if they differ in paths for action or
// origin.
TEST_F(PasswordFormManagerTest, TestIgnoreResult_Paths) {
  PasswordForm observed(*observed_form());
  observed.origin = GURL("https://accounts.google.com/a/LoginAuth");
  observed.action = GURL("https://accounts.google.com/a/Login");
  observed.signon_realm = "https://accounts.google.com";

  FakeFormFetcher fetcher;
  fetcher.Fetch();
  PasswordFormManager form_manager(password_manager(), client(),
                                   client()->driver(), observed,
                                   std::make_unique<MockFormSaver>(), &fetcher);
  form_manager.Init(nullptr);

  PasswordForm saved_form = observed;
  saved_form.origin = GURL("https://accounts.google.com/a/OtherLoginAuth");
  saved_form.action = GURL("https://accounts.google.com/a/OtherLogin");
  fetcher.SetNonFederated({&saved_form}, 0u);

  // Different paths for action / origin are okay.
  EXPECT_EQ(1u, form_manager.GetBestMatches().size());
  EXPECT_EQ(*form_manager.GetBestMatches().begin()->second, saved_form);
}

// Test that saved empty action URL is updated with the submitted action URL.
TEST_F(PasswordFormManagerTest, TestEmptyAction) {
  saved_match()->action = GURL();
  fake_form_fetcher()->SetNonFederated({saved_match()}, 0u);

  // User logs in with the autofilled username / password from saved_match.
  PasswordForm login = *observed_form();
  login.username_value = saved_match()->username_value;
  login.password_value = saved_match()->password_value;
  form_manager()->ProvisionallySave(login);
  EXPECT_FALSE(form_manager()->IsNewLogin());
  // Chrome updates the saved PasswordForm entry with the action URL of the
  // observed form.
  EXPECT_EQ(observed_form()->action,
            form_manager()->GetPendingCredentials().action);
}

TEST_F(PasswordFormManagerTest, TestUpdateAction) {
  saved_match()->action = GURL("http://accounts.google.com/a/ServiceLogin");
  fake_form_fetcher()->SetNonFederated({saved_match()}, 0u);

  // User logs in with the autofilled username / password from saved_match.
  observed_form()->action = GURL("http://accounts.google.com/a/Login");
  PasswordForm login = *observed_form();
  login.username_value = saved_match()->username_value;
  login.password_value = saved_match()->password_value;

  form_manager()->ProvisionallySave(login);
  EXPECT_FALSE(form_manager()->IsNewLogin());
  // The observed action URL is different from the previously saved one. Chrome
  // should update the store by setting the pending credential's action URL to
  // be that of the currently observed form.
  EXPECT_EQ(observed_form()->action,
            form_manager()->GetPendingCredentials().action);
}

TEST_F(PasswordFormManagerTest, TestDynamicAction) {
  fake_form_fetcher()->SetNonFederated(std::vector<const PasswordForm*>(), 0u);

  observed_form()->action = GURL("http://accounts.google.com/a/Login");
  PasswordForm login(*observed_form());
  // The submitted action URL is different from the one observed on page load.
  login.action = GURL("http://www.google.com/new_action");

  form_manager()->ProvisionallySave(login);
  EXPECT_TRUE(form_manager()->IsNewLogin());
  // Check that the provisionally saved action URL is the same as the submitted
  // action URL, not the one observed on page load.
  EXPECT_EQ(login.action, form_manager()->GetPendingCredentials().action);
}

// Test that if the saved match has other possible usernames stored, and the
// user chooses the main one, then the other possible usernames are dropped on
// update.
TEST_F(PasswordFormManagerTest, TestAlternateUsername_NoChange) {
  EXPECT_CALL(*client()->mock_driver(), AllowPasswordGenerationForForm(_));

  PasswordForm saved_form = *saved_match();
  saved_form.other_possible_usernames.push_back(
      ValueElementPair(ASCIIToUTF16("other_possible@gmail.com"),
                       ASCIIToUTF16("other_username")));

  fake_form_fetcher()->SetNonFederated({&saved_form}, 0u);

  // The saved match has the right username already.
  PasswordForm login(*observed_form());
  login.preferred = true;
  login.username_value = saved_match()->username_value;
  login.password_value = saved_match()->password_value;

  form_manager()->ProvisionallySave(login);

  EXPECT_FALSE(form_manager()->IsNewLogin());

  PasswordForm saved_result;
  EXPECT_CALL(MockFormSaver::Get(form_manager()), Update(_, _, _, nullptr))
      .WillOnce(SaveArg<0>(&saved_result));
  EXPECT_CALL(*client()->mock_driver()->mock_autofill_download_manager(),
              StartUploadRequest(_, false, _, _, true, nullptr));

  form_manager()->Save();
  // Should be only one password stored, and should not have
  // |other_possible_usernames| set anymore.
  EXPECT_EQ(saved_match()->username_value, saved_result.username_value);
  EXPECT_TRUE(saved_result.other_possible_usernames.empty());
}

TEST_F(PasswordFormManagerTest, TestSendNotBlacklistedMessage_NoCredentials) {
  // First time sign-up attempt. Password store does not contain matching
  // credentials. AllowPasswordGenerationForForm should be called to send the
  // "not blacklisted" message.
  EXPECT_CALL(*(client()->mock_driver()), AllowPasswordGenerationForForm(_));
  fake_form_fetcher()->SetNonFederated(std::vector<const PasswordForm*>(), 0u);
}

TEST_F(PasswordFormManagerTest, TestSendNotBlacklistedMessage_Credentials) {
  // Signing up on a previously visited site. Credentials are found in the
  // password store, and are not blacklisted. AllowPasswordGenerationForForm
  // should be called to send the "not blacklisted" message.
  EXPECT_CALL(*(client()->mock_driver()), AllowPasswordGenerationForForm(_));
  PasswordForm simulated_result = CreateSavedMatch(false);
  fake_form_fetcher()->SetNonFederated({&simulated_result}, 0u);
}

TEST_F(PasswordFormManagerTest,
       TestSendNotBlacklistedMessage_DroppedCredentials) {
  // There are cases, such as when a form is made explicitly for creating a new
  // password, where we may ignore saved credentials. Make sure that we still
  // allow generation in that case.
  PasswordForm signup_form(*observed_form());
  signup_form.new_password_element = base::ASCIIToUTF16("new_password_field");

  FakeFormFetcher fetcher;
  fetcher.Fetch();
  PasswordFormManager form_manager(password_manager(), client(),
                                   client()->driver(), signup_form,
                                   std::make_unique<MockFormSaver>(), &fetcher);
  form_manager.Init(nullptr);
  EXPECT_CALL(*(client()->mock_driver()), AllowPasswordGenerationForForm(_));
  PasswordForm simulated_result = CreateSavedMatch(false);
  fetcher.SetNonFederated({&simulated_result}, 0u);
}

// Test that exactly one match for each username is chosen as a best match, even
// though it is a PSL match.
TEST_F(PasswordFormManagerTest, TestBestCredentialsForEachUsernameAreIncluded) {
  // Add a best scoring match. It should be in |best_matches| and chosen as a
  // prefferred match.
  PasswordForm best_scoring = *saved_match();

  // Add a match saved on another form, it has lower score. It should not be in
  // |best_matches|.
  PasswordForm other_form = *saved_match();
  other_form.password_element = ASCIIToUTF16("signup_password");
  other_form.username_element = ASCIIToUTF16("signup_username");

  // Add a match saved on another form with a different username. It should be
  // in |best_matches|.
  PasswordForm other_username = other_form;
  const base::string16 kUsername1 =
      other_username.username_value + ASCIIToUTF16("1");
  other_username.username_value = kUsername1;

  // Add a PSL match, it should not be in |best_matches|.
  PasswordForm psl_match = *psl_saved_match();

  // Add a PSL match with a different username. It should be in |best_matches|.
  PasswordForm psl_match_other = psl_match;
  const base::string16 kUsername2 =
      psl_match_other.username_value + ASCIIToUTF16("2");
  psl_match_other.username_value = kUsername2;

  autofill::PasswordFormFillData fill_data;
  EXPECT_CALL(*client()->mock_driver(), FillPasswordForm(_))
      .WillOnce(SaveArg<0>(&fill_data));

  fake_form_fetcher()->SetNonFederated(
      {&best_scoring, &other_form, &other_username, &psl_match,
       &psl_match_other},
      0u);

  const std::map<base::string16, const PasswordForm*>& best_matches =
      form_manager()->GetBestMatches();
  EXPECT_EQ(3u, best_matches.size());
  EXPECT_NE(best_matches.end(),
            best_matches.find(saved_match()->username_value));
  EXPECT_EQ(*saved_match(),
            *best_matches.find(saved_match()->username_value)->second);
  EXPECT_NE(best_matches.end(), best_matches.find(kUsername1));
  EXPECT_NE(best_matches.end(), best_matches.find(kUsername2));

  EXPECT_EQ(*saved_match(), *form_manager()->GetPreferredMatch());
  EXPECT_EQ(2u, fill_data.additional_logins.size());
}

TEST_F(PasswordFormManagerTest, TestSanitizePossibleUsernames) {
  const ValueElementPair kUsernameOther(ASCIIToUTF16("other username"),
                                        ASCIIToUTF16("other_username_id"));

  fake_form_fetcher()->SetNonFederated(std::vector<const PasswordForm*>(), 0u);

  PasswordForm credentials(*observed_form());
  credentials.other_possible_usernames.push_back(
      ValueElementPair(ASCIIToUTF16("543-43-1234"), ASCIIToUTF16("id1")));
  credentials.other_possible_usernames.push_back(
      ValueElementPair(ASCIIToUTF16("378282246310005"), ASCIIToUTF16("id2")));
  credentials.other_possible_usernames.push_back(kUsernameOther);
  credentials.username_value = ASCIIToUTF16("test@gmail.com");
  credentials.preferred = true;

  form_manager()->ProvisionallySave(credentials);

  PasswordForm saved_result;
  EXPECT_CALL(MockFormSaver::Get(form_manager()), Save(_, _))
      .WillOnce(SaveArg<0>(&saved_result));
  form_manager()->Save();

  // Possible credit card number and SSN are stripped.
  EXPECT_THAT(saved_result.other_possible_usernames,
              UnorderedElementsAre(kUsernameOther));
}

TEST_F(PasswordFormManagerTest, TestSanitizePossibleUsernamesDuplicates) {
  const ValueElementPair kUsernameSsn(ASCIIToUTF16("511-32-9830"),
                                      ASCIIToUTF16("ssn_id"));
  const ValueElementPair kUsernameEmail(ASCIIToUTF16("test@gmail.com"),
                                        ASCIIToUTF16("email_id"));
  const ValueElementPair kUsernameDuplicate(ASCIIToUTF16("duplicate"),
                                            ASCIIToUTF16("duplicate_id"));
  const ValueElementPair kUsernameRandom(ASCIIToUTF16("random"),
                                         ASCIIToUTF16("random_id"));

  fake_form_fetcher()->SetNonFederated(std::vector<const PasswordForm*>(), 0u);

  PasswordForm credentials(*observed_form());
  credentials.other_possible_usernames.push_back(kUsernameSsn);
  credentials.other_possible_usernames.push_back(kUsernameDuplicate);
  credentials.other_possible_usernames.push_back(kUsernameDuplicate);
  credentials.other_possible_usernames.push_back(kUsernameRandom);
  credentials.other_possible_usernames.push_back(kUsernameEmail);
  credentials.username_value = kUsernameEmail.first;
  credentials.preferred = true;

  form_manager()->ProvisionallySave(credentials);

  PasswordForm saved_result;
  EXPECT_CALL(MockFormSaver::Get(form_manager()), Save(_, _))
      .WillOnce(SaveArg<0>(&saved_result));
  form_manager()->Save();

  // SSN, duplicate in |other_possible_usernames| and duplicate of
  // |username_value| all removed.
  EXPECT_THAT(saved_result.other_possible_usernames,
              UnorderedElementsAre(kUsernameDuplicate, kUsernameRandom));
}

TEST_F(PasswordFormManagerTest, TestAllPossiblePasswords) {
  fake_form_fetcher()->SetNonFederated(std::vector<const PasswordForm*>(), 0u);

  ValueElementPair pair1 = {ASCIIToUTF16("pass1"), ASCIIToUTF16("el1")};
  ValueElementPair pair2 = {ASCIIToUTF16("pass2"), ASCIIToUTF16("el2")};
  ValueElementPair pair3 = {ASCIIToUTF16("pass3"), ASCIIToUTF16("el3")};

  PasswordForm credentials(*observed_form());
  credentials.all_possible_passwords.push_back(pair1);
  credentials.all_possible_passwords.push_back(pair2);
  credentials.all_possible_passwords.push_back(pair3);

  form_manager()->ProvisionallySave(credentials);

  EXPECT_THAT(form_manager()->GetPendingCredentials().all_possible_passwords,
              UnorderedElementsAre(pair1, pair2, pair3));
}

// Test that public-suffix-matched credentials score lower than same-origin
// ones.
TEST_F(PasswordFormManagerTest, TestScoringPublicSuffixMatch) {
  EXPECT_CALL(*(client()->mock_driver()), AllowPasswordGenerationForForm(_));

  PasswordForm base_match = CreateSavedMatch(false);
  base_match.origin = GURL("http://accounts.google.com/a/ServiceLoginAuth");
  base_match.action = GURL("http://accounts.google.com/a/ServiceLogin");

  PasswordForm psl_match = base_match;
  psl_match.is_public_suffix_match = true;

  // Change origin and action URLs to decrease the score.
  PasswordForm same_origin_match = base_match;
  psl_match.origin = GURL("http://accounts.google.com/a/ServiceLoginAuth2");
  psl_match.action = GURL("http://accounts.google.com/a/ServiceLogin2");

  autofill::PasswordFormFillData fill_data;
  EXPECT_CALL(*client()->mock_driver(), FillPasswordForm(_))
      .WillOnce(SaveArg<0>(&fill_data));

  fake_form_fetcher()->SetNonFederated({&psl_match, &same_origin_match}, 0u);
  EXPECT_TRUE(fill_data.additional_logins.empty());
  EXPECT_EQ(1u, form_manager()->GetBestMatches().size());
  EXPECT_FALSE(
      form_manager()->GetBestMatches().begin()->second->is_public_suffix_match);
}

TEST_F(PasswordFormManagerTest, AndroidCredentialsAreAutofilled) {
  EXPECT_CALL(*(client()->mock_driver()), AllowPasswordGenerationForForm(_));

  // Although Android-based credentials are treated similarly to PSL-matched
  // credentials in some respects, they should be autofilled as opposed to be
  // filled on username-select.
  PasswordForm android_login;
  android_login.signon_realm = "android://hash@com.google.android";
  android_login.origin = GURL("android://hash@com.google.android/");
  android_login.is_affiliation_based_match = true;
  android_login.username_value = saved_match()->username_value;
  android_login.password_value = saved_match()->password_value;
  android_login.preferred = false;
  android_login.times_used = 42;

  autofill::PasswordFormFillData fill_data;
  EXPECT_CALL(*client()->mock_driver(), FillPasswordForm(_))
      .WillOnce(SaveArg<0>(&fill_data));

  fake_form_fetcher()->SetNonFederated({&android_login}, 0u);
  EXPECT_TRUE(fill_data.additional_logins.empty());
  EXPECT_FALSE(fill_data.wait_for_username);
  EXPECT_EQ(1u, form_manager()->GetBestMatches().size());

  // When the user submits the filled form, no copy of the credential should be
  // created, instead the usage counter of the original credential should be
  // incremented in-place, as if it were a regular credential for that website.
  PasswordForm credential(*observed_form());
  credential.username_value = android_login.username_value;
  credential.password_value = android_login.password_value;
  credential.preferred = true;
  form_manager()->ProvisionallySave(credential);
  EXPECT_FALSE(form_manager()->IsNewLogin());

  PasswordForm updated_credential;
  EXPECT_CALL(MockFormSaver::Get(form_manager()), Update(_, _, _, nullptr))
      .WillOnce(testing::SaveArg<0>(&updated_credential));
  form_manager()->Save();

  EXPECT_EQ(android_login.username_value, updated_credential.username_value);
  EXPECT_EQ(android_login.password_value, updated_credential.password_value);
  EXPECT_EQ(android_login.times_used + 1, updated_credential.times_used);
  EXPECT_TRUE(updated_credential.preferred);
  EXPECT_EQ(GURL(), updated_credential.action);
  EXPECT_EQ(base::string16(), updated_credential.username_element);
  EXPECT_EQ(base::string16(), updated_credential.password_element);
  EXPECT_EQ(base::string16(), updated_credential.submit_element);
}

// Credentials saved through Android apps should always be shown in the drop-
// down menu, unless there is a better-scoring match with the same username.
TEST_F(PasswordFormManagerTest, AndroidCredentialsAreProtected) {
  const char kTestUsername1[] = "test-user@gmail.com";
  const char kTestUsername2[] = "test-other-user@gmail.com";
  const char kTestWebPassword[] = "web-password";
  const char kTestAndroidPassword1[] = "android-password-alpha";
  const char kTestAndroidPassword2[] = "android-password-beta";

  EXPECT_CALL(*(client()->mock_driver()), AllowPasswordGenerationForForm(_));

  // Suppose there is one login saved through the website, and two other coming
  // from Android: the first has the same username as the web-based credential,
  // so it should be suppressed, but the second has a different username, so it
  // should be shown.
  PasswordForm website_login = CreateSavedMatch(false);
  website_login.username_value = ASCIIToUTF16(kTestUsername1);
  website_login.password_value = ASCIIToUTF16(kTestWebPassword);

  PasswordForm android_same;
  android_same.signon_realm = "android://hash@com.google.android";
  android_same.origin = GURL("android://hash@com.google.android/");
  android_same.username_value = ASCIIToUTF16(kTestUsername1);
  android_same.password_value = ASCIIToUTF16(kTestAndroidPassword1);

  PasswordForm android_other = android_same;
  android_other.username_value = ASCIIToUTF16(kTestUsername2);
  android_other.password_value = ASCIIToUTF16(kTestAndroidPassword2);

  std::vector<std::unique_ptr<PasswordForm>> expected_matches;
  expected_matches.push_back(std::make_unique<PasswordForm>(website_login));
  expected_matches.push_back(std::make_unique<PasswordForm>(android_other));

  autofill::PasswordFormFillData fill_data;
  EXPECT_CALL(*client()->mock_driver(), FillPasswordForm(_))
      .WillOnce(SaveArg<0>(&fill_data));
  fake_form_fetcher()->SetNonFederated(
      {&website_login, &android_same, &android_other}, 0u);

  EXPECT_FALSE(fill_data.wait_for_username);
  EXPECT_EQ(1u, fill_data.additional_logins.size());

  std::vector<std::unique_ptr<PasswordForm>> actual_matches;
  for (const auto& username_match_pair : form_manager()->GetBestMatches())
    actual_matches.push_back(
        std::make_unique<PasswordForm>(*username_match_pair.second));
  EXPECT_THAT(actual_matches,
              UnorderedPasswordFormElementsAre(&expected_matches));
}

TEST_F(PasswordFormManagerTest, InvalidActionURLsDoNotMatch) {
  PasswordForm invalid_action_form(*observed_form());
  invalid_action_form.action = GURL("http://");
  ASSERT_FALSE(invalid_action_form.action.is_valid());
  ASSERT_FALSE(invalid_action_form.action.is_empty());
  // Non-empty invalid action URLs should not match other actions.
  // First when the compared form has an invalid URL:
  EXPECT_EQ(0, form_manager()->DoesManage(invalid_action_form, nullptr) &
                   PasswordFormManager::RESULT_ACTION_MATCH);
  // Then when the observed form has an invalid URL:
  PasswordForm valid_action_form(*observed_form());
  PasswordFormManager invalid_manager(
      password_manager(), client(), client()->driver(), invalid_action_form,
      std::make_unique<MockFormSaver>(), fake_form_fetcher());
  invalid_manager.Init(nullptr);
  EXPECT_EQ(0, invalid_manager.DoesManage(valid_action_form, nullptr) &
                   PasswordFormManager::RESULT_ACTION_MATCH);
}

TEST_F(PasswordFormManagerTest, EmptyActionURLsDoNotMatchNonEmpty) {
  PasswordForm empty_action_form(*observed_form());
  empty_action_form.action = GURL();
  ASSERT_FALSE(empty_action_form.action.is_valid());
  ASSERT_TRUE(empty_action_form.action.is_empty());
  // First when the compared form has an empty URL:
  EXPECT_EQ(0, form_manager()->DoesManage(empty_action_form, nullptr) &
                   PasswordFormManager::RESULT_ACTION_MATCH);
  // Then when the observed form has an empty URL:
  PasswordForm valid_action_form(*observed_form());
  PasswordFormManager empty_action_manager(
      password_manager(), client(), client()->driver(), empty_action_form,
      std::make_unique<MockFormSaver>(), fake_form_fetcher());
  empty_action_manager.Init(nullptr);
  EXPECT_EQ(0, empty_action_manager.DoesManage(valid_action_form, nullptr) &
                   PasswordFormManager::RESULT_ACTION_MATCH);
}

TEST_F(PasswordFormManagerTest, NonHTMLFormsDoNotMatchHTMLForms) {
  ASSERT_EQ(PasswordForm::SCHEME_HTML, observed_form()->scheme);
  PasswordForm non_html_form(*observed_form());
  non_html_form.scheme = PasswordForm::SCHEME_DIGEST;
  EXPECT_EQ(PasswordFormManager::RESULT_NO_MATCH,
            form_manager()->DoesManage(non_html_form, nullptr));

  // The other way round: observing a non-HTML form, don't match a HTML form.
  PasswordForm html_form(*observed_form());
  PasswordFormManager non_html_manager(
      password_manager(), client(), kNoDriver, non_html_form,
      std::make_unique<MockFormSaver>(), fake_form_fetcher());
  non_html_manager.Init(nullptr);
  EXPECT_EQ(PasswordFormManager::RESULT_NO_MATCH,
            non_html_manager.DoesManage(html_form, nullptr));
}

TEST_F(PasswordFormManagerTest, OriginCheck_HostsMatchExactly) {
  // Host part of origins must match exactly, not just by prefix.
  PasswordForm form_longer_host(*observed_form());
  form_longer_host.origin = GURL("http://accounts.google.com.au/a/LoginAuth");
  // Check that accounts.google.com does not match accounts.google.com.au.
  EXPECT_EQ(PasswordFormManager::RESULT_NO_MATCH,
            form_manager()->DoesManage(form_longer_host, nullptr));
}

TEST_F(PasswordFormManagerTest, OriginCheck_MoreSecureSchemePathsMatchPrefix) {
  // If the URL scheme of the observed form is HTTP, and the compared form is
  // HTTPS, then the compared form can extend the path.
  PasswordForm form_longer_path(*observed_form());
  form_longer_path.origin = GURL("https://accounts.google.com/a/LoginAuth/sec");
  EXPECT_NE(0, form_manager()->DoesManage(form_longer_path, nullptr) &
                   PasswordFormManager::RESULT_ORIGINS_OR_FRAMES_MATCH);
}

TEST_F(PasswordFormManagerTest,
       OriginCheck_NotMoreSecureSchemePathsMatchExactly) {
  // If the origin URL scheme of the compared form is not more secure than that
  // of the observed form, then the paths must match exactly.
  PasswordForm form_longer_path(*observed_form());
  form_longer_path.origin = GURL("http://accounts.google.com/a/LoginAuth/sec");
  // Check that /a/LoginAuth does not match /a/LoginAuth/more.
  EXPECT_EQ(PasswordFormManager::RESULT_NO_MATCH,
            form_manager()->DoesManage(form_longer_path, nullptr));

  PasswordForm secure_observed_form(*observed_form());
  secure_observed_form.origin = GURL("https://accounts.google.com/a/LoginAuth");
  PasswordFormManager secure_manager(
      password_manager(), client(), client()->driver(), secure_observed_form,
      std::make_unique<MockFormSaver>(), fake_form_fetcher());
  secure_manager.Init(nullptr);
  // Also for HTTPS in the observed form, and HTTP in the compared form, an
  // exact path match is expected.
  EXPECT_EQ(PasswordFormManager::RESULT_NO_MATCH,
            secure_manager.DoesManage(form_longer_path, nullptr));
  // Not even upgrade to HTTPS in the compared form should help.
  form_longer_path.origin = GURL("https://accounts.google.com/a/LoginAuth/sec");
  EXPECT_EQ(PasswordFormManager::RESULT_NO_MATCH,
            secure_manager.DoesManage(form_longer_path, nullptr));
}

TEST_F(PasswordFormManagerTest, OriginCheck_OnlyOriginsMatch) {
  // Make sure DoesManage() can distinguish when only origins match.

  PasswordForm same_origin_only(*observed_form());
  same_origin_only.form_data.name = ASCIIToUTF16("other_name");
  same_origin_only.action = GURL("https://somewhere/else");

  EXPECT_EQ(PasswordFormManager::RESULT_ORIGINS_OR_FRAMES_MATCH,
            form_manager()->DoesManage(same_origin_only, nullptr));
}

TEST_F(PasswordFormManagerTest, FormsMatchIfNamesMatch) {
  PasswordForm other_form(*observed_form());
  autofill::FormFieldData field;
  field.name = ASCIIToUTF16("another-field-name");
  other_form.form_data.fields.push_back(field);
  other_form.action = GURL("https://somewhere/else");
  // Names should match, other things may not.
  EXPECT_EQ(PasswordFormManager::RESULT_FORM_NAME_MATCH,
            form_manager()->DoesManage(other_form, nullptr) &
                PasswordFormManager::RESULT_FORM_NAME_MATCH);
}

TEST_F(PasswordFormManagerTest, FormsMatchIfSignaturesMatch) {
  PasswordForm other_form(*observed_form());
  other_form.action = GURL("https://somewhere/else");
  // Signatures should match, other things may not.
  EXPECT_EQ(PasswordFormManager::RESULT_SIGNATURE_MATCH,
            form_manager()->DoesManage(other_form, nullptr) &
                PasswordFormManager::RESULT_SIGNATURE_MATCH);
}

TEST_F(PasswordFormManagerTest, FormWithEmptyActionAndNameMatchesItself) {
  observed_form()->form_data.name.clear();
  observed_form()->action = GURL::EmptyGURL();
  PasswordFormManager form_manager(
      password_manager(), client(), client()->driver(), *observed_form(),
      std::make_unique<NiceMock<MockFormSaver>>(), fake_form_fetcher());
  form_manager.Init(nullptr);
  // Any form should match itself regardless of missing properties. Otherwise,
  // a PasswordFormManager instance is created for the same form multiple times.
  PasswordForm other_form(*observed_form());
  EXPECT_EQ(PasswordFormManager::RESULT_COMPLETE_MATCH,
            form_manager.DoesManage(other_form, nullptr));
}

// Test that if multiple credentials with the same username are stored, and the
// user updates the password, then all of the stored passwords get updated as
// long as they have the same password value.
TEST_F(PasswordFormManagerTest, CorrectlyUpdatePasswordsWithSameUsername) {
  EXPECT_CALL(*client()->mock_driver(), AllowPasswordGenerationForForm(_));

  PasswordForm first(*saved_match());
  first.action = observed_form()->action;
  first.password_value = ASCIIToUTF16("first");
  first.preferred = true;

  // The second credential has the same password value, but it has a different
  // |username_element| to make a different unique key for the database
  // (otherwise the two credentials could not be stored at the same time). The
  // different unique key results in a slightly lower score than for |first|.
  PasswordForm second(first);
  second.username_element.clear();
  second.preferred = false;

  // The third credential has a different password value. It also has a
  // different |password_element| to make a different unique key for the
  // database again.
  PasswordForm third(first);
  third.password_element.clear();
  third.password_value = ASCIIToUTF16("second");
  third.preferred = false;

  fake_form_fetcher()->SetNonFederated({&first, &second, &third}, 0u);

  // |first| scored slightly higher.
  EXPECT_EQ(ASCIIToUTF16("first"),
            form_manager()->GetPreferredMatch()->password_value);

  PasswordForm login(*observed_form());
  login.username_value = saved_match()->username_value;
  login.password_value = ASCIIToUTF16("third");
  login.preferred = true;
  form_manager()->ProvisionallySave(login);

  EXPECT_FALSE(form_manager()->IsNewLogin());

  PasswordForm saved_result;
  std::vector<PasswordForm> credentials_to_update;
  EXPECT_CALL(MockFormSaver::Get(form_manager()), Update(_, _, _, nullptr))
      .WillOnce(testing::DoAll(SaveArg<0>(&saved_result),
                               SaveArgPointee<2>(&credentials_to_update)));
  EXPECT_CALL(*client()->mock_driver()->mock_autofill_download_manager(),
              StartUploadRequest(_, false, _, _, true, nullptr));

  form_manager()->Save();

  // What was |first| above should be the main credential updated.
  EXPECT_EQ(ASCIIToUTF16("third"), saved_result.password_value);
  EXPECT_FALSE(saved_result.password_element.empty());
  EXPECT_FALSE(saved_result.username_element.empty());

  // What was |second| above should be another credential updated.
  ASSERT_EQ(1u, credentials_to_update.size());
  EXPECT_EQ(ASCIIToUTF16("third"), credentials_to_update[0].password_value);
  EXPECT_FALSE(credentials_to_update[0].password_element.empty());
  EXPECT_TRUE(credentials_to_update[0].username_element.empty());
}

TEST_F(PasswordFormManagerTest, UploadFormData_NewPassword) {
  // For newly saved passwords, upload a password vote for autofill::PASSWORD.
  // Don't vote for the username field yet.
  FakeFormFetcher fetcher;
  fetcher.Fetch();
  PasswordFormManager form_manager(
      password_manager(), client(), client()->driver(), *saved_match(),
      std::make_unique<NiceMock<MockFormSaver>>(), &fetcher);
  form_manager.Init(nullptr);
  fetcher.SetNonFederated(std::vector<const PasswordForm*>(), 0u);

  PasswordForm form_to_save(*saved_match());
  form_to_save.preferred = true;
  form_to_save.username_value = ASCIIToUTF16("username");
  form_to_save.password_value = ASCIIToUTF16("1234");

  autofill::ServerFieldTypeSet expected_available_field_types;
  expected_available_field_types.insert(autofill::PASSWORD);
  EXPECT_CALL(*client()->mock_driver()->mock_autofill_download_manager(),
              StartUploadRequest(_, false, expected_available_field_types, _,
                                 true, nullptr));
  form_manager.ProvisionallySave(form_to_save);
  form_manager.Save();
}

TEST_F(PasswordFormManagerTest, UploadFormData_NewPassword_Blacklist) {
  // Do not upload a vote if the user is blacklisting the form.
  FakeFormFetcher fetcher;
  fetcher.Fetch();
  PasswordFormManager blacklist_form_manager(
      password_manager(), client(), client()->driver(), *saved_match(),
      std::make_unique<NiceMock<MockFormSaver>>(), &fetcher);
  blacklist_form_manager.Init(nullptr);
  fetcher.SetNonFederated(std::vector<const PasswordForm*>(), 0u);

  autofill::ServerFieldTypeSet expected_available_field_types;
  expected_available_field_types.insert(autofill::USERNAME);
  expected_available_field_types.insert(autofill::PASSWORD);
  EXPECT_CALL(
      *client()->mock_driver()->mock_autofill_download_manager(),
      StartUploadRequest(_, _, expected_available_field_types, _, true, _))
      .Times(0);
  blacklist_form_manager.PermanentlyBlacklist();
}

TEST_F(PasswordFormManagerTest, UploadPasswordForm) {
  autofill::FormData observed_form_data;
  autofill::FormFieldData field;
  field.label = ASCIIToUTF16("Email:");
  field.name = ASCIIToUTF16("observed-username-field");
  field.form_control_type = "text";
  observed_form_data.fields.push_back(field);

  field.label = ASCIIToUTF16("Password:");
  field.name = ASCIIToUTF16("observed-password-field");
  field.form_control_type = "password";
  observed_form_data.fields.push_back(field);

  // Form data is different than saved form data, account creation signal should
  // be sent.
  autofill::ServerFieldType field_type = autofill::ACCOUNT_CREATION_PASSWORD;
  AccountCreationUploadTest(observed_form_data, 0, PasswordForm::NO_SIGNAL_SENT,
                            &field_type);

  // Non-zero times used will not upload since we only upload a positive signal
  // at most once.
  AccountCreationUploadTest(observed_form_data, 1, PasswordForm::NO_SIGNAL_SENT,
                            nullptr);

  // Same form data as saved match and POSITIVE_SIGNAL_SENT means there should
  // be a negative autofill ping sent.
  field_type = autofill::NOT_ACCOUNT_CREATION_PASSWORD;
  AccountCreationUploadTest(saved_match()->form_data, 2,
                            PasswordForm::POSITIVE_SIGNAL_SENT, &field_type);

  // For any other GenerationUploadStatus, no autofill upload should occur
  // if the observed form data matches the saved form data.
  AccountCreationUploadTest(saved_match()->form_data, 3,
                            PasswordForm::NO_SIGNAL_SENT, nullptr);
  AccountCreationUploadTest(saved_match()->form_data, 3,
                            PasswordForm::NEGATIVE_SIGNAL_SENT, nullptr);
}

TEST_F(PasswordFormManagerTest, CorrectlySavePasswordWithoutUsernameFields) {
  EXPECT_CALL(*client()->mock_driver(), AllowPasswordGenerationForForm(_));

  fake_form_fetcher()->SetNonFederated(std::vector<const PasswordForm*>(), 0u);

  PasswordForm login(*observed_form());
  login.username_element.clear();
  login.password_value = ASCIIToUTF16("password");
  login.preferred = true;

  form_manager()->ProvisionallySave(login);

  EXPECT_TRUE(form_manager()->IsNewLogin());

  PasswordForm saved_result;
  EXPECT_CALL(MockFormSaver::Get(form_manager()), Save(_, _))
      .WillOnce(SaveArg<0>(&saved_result));

  form_manager()->Save();

  // Make sure that the password is updated appropriately.
  EXPECT_EQ(ASCIIToUTF16("password"), saved_result.password_value);
}

TEST_F(PasswordFormManagerTest, DriverDeletedBeforeStoreDone) {
  // Test graceful handling of the following situation:
  // 1. A form appears in a frame, a PFM is created for that form.
  // 2. The PFM asks the store for credentials for this form.
  // 3. The frame (and associated driver) gets deleted.
  // 4. The PFM returns the callback with credentials.
  // This test checks implicitly that after step 4 the PFM does not attempt
  // use-after-free of the deleted driver.
  std::string example_url("http://example.com");
  PasswordForm form;
  form.origin = GURL(example_url);
  form.signon_realm = example_url;
  form.action = GURL(example_url);
  form.username_element = ASCIIToUTF16("u");
  form.password_element = ASCIIToUTF16("p");
  form.submit_element = ASCIIToUTF16("s");

  FakeFormFetcher fetcher;
  fetcher.Fetch();
  PasswordFormManager form_manager(password_manager(), client(),
                                   client()->driver(), form,
                                   std::make_unique<MockFormSaver>(), &fetcher);
  form_manager.Init(nullptr);

  // Suddenly, the frame and its driver disappear.
  client()->KillDriver();

  fetcher.SetNonFederated({&form}, 0u);
}

TEST_F(PasswordFormManagerTest, PreferredMatchIsUpToDate) {
  // Check that GetPreferredMatch() is always a member of GetBestMatches().
  PasswordForm form = *observed_form();
  form.username_value = ASCIIToUTF16("username");
  form.password_value = ASCIIToUTF16("password1");
  form.preferred = false;

  PasswordForm generated_form = form;
  generated_form.type = PasswordForm::TYPE_GENERATED;
  generated_form.password_value = ASCIIToUTF16("password2");
  generated_form.preferred = true;

  fake_form_fetcher()->SetNonFederated({&form, &generated_form}, 0u);

  EXPECT_EQ(1u, form_manager()->GetBestMatches().size());
  EXPECT_EQ(form_manager()->GetPreferredMatch(),
            form_manager()->GetBestMatches().begin()->second);
  // Make sure to access all fields of preferred_match; this way if it was
  // deleted, ASAN might notice it.
  PasswordForm dummy(*form_manager()->GetPreferredMatch());
}

TEST_F(PasswordFormManagerTest, PasswordToSave_NoElements) {
  PasswordForm form;
  EXPECT_TRUE(PasswordFormManager::PasswordToSave(form).first.empty());
}

TEST_F(PasswordFormManagerTest, PasswordToSave_NoNewElement) {
  PasswordForm form;
  form.password_element = base::ASCIIToUTF16("pwd");
  base::string16 kValue = base::ASCIIToUTF16("val");
  form.password_value = kValue;
  EXPECT_EQ(kValue, PasswordFormManager::PasswordToSave(form).first);
}

TEST_F(PasswordFormManagerTest, PasswordToSave_NoOldElement) {
  PasswordForm form;
  form.new_password_element = base::ASCIIToUTF16("new_pwd");
  base::string16 kNewValue = base::ASCIIToUTF16("new_val");
  form.new_password_value = kNewValue;
  EXPECT_EQ(kNewValue, PasswordFormManager::PasswordToSave(form).first);
}

TEST_F(PasswordFormManagerTest, PasswordToSave_BothButNoNewValue) {
  PasswordForm form;
  form.password_element = base::ASCIIToUTF16("pwd");
  form.new_password_element = base::ASCIIToUTF16("new_pwd");
  base::string16 kValue = base::ASCIIToUTF16("val");
  form.password_value = kValue;
  EXPECT_EQ(kValue, PasswordFormManager::PasswordToSave(form).first);
}

TEST_F(PasswordFormManagerTest, PasswordToSave_NewValue) {
  PasswordForm form;
  form.password_element = base::ASCIIToUTF16("pwd");
  form.new_password_element = base::ASCIIToUTF16("new_pwd");
  form.password_value = base::ASCIIToUTF16("val");
  base::string16 kNewValue = base::ASCIIToUTF16("new_val");
  form.new_password_value = kNewValue;
  EXPECT_EQ(kNewValue, PasswordFormManager::PasswordToSave(form).first);
}

TEST_F(PasswordFormManagerTest, TestSuggestingPasswordChangeForms) {
  // Suggesting password on the password change form on the previously visited
  // site. Credentials are found in the password store, and are not blacklisted.
  PasswordForm observed_change_password_form = *observed_form();
  observed_change_password_form.new_password_element =
      base::ASCIIToUTF16("new_pwd");
  FakeFormFetcher fetcher;
  fetcher.Fetch();
  PasswordFormManager manager_creds(
      password_manager(), client(), client()->driver(),
      observed_change_password_form, std::make_unique<MockFormSaver>(),
      &fetcher);
  manager_creds.Init(nullptr);

  autofill::PasswordFormFillData fill_data;
  EXPECT_CALL(*client()->mock_driver(), FillPasswordForm(_))
      .WillOnce(SaveArg<0>(&fill_data));

  PasswordForm result = CreateSavedMatch(false);
  fetcher.SetNonFederated({&result}, 0u);
  EXPECT_EQ(1u, manager_creds.GetBestMatches().size());
  EXPECT_EQ(0u, fill_data.additional_logins.size());
  EXPECT_TRUE(fill_data.wait_for_username);
}

TEST_F(PasswordFormManagerTest, TestUpdateMethod) {
  // Add a new password field to the test form. The PasswordFormManager should
  // save the password from this field, instead of the current password field.
  observed_form()->new_password_element = ASCIIToUTF16("NewPasswd");
  autofill::FormFieldData field;
  field.label = ASCIIToUTF16("NewPasswd");
  field.name = ASCIIToUTF16("NewPasswd");
  field.form_control_type = "password";
  observed_form()->form_data.fields.push_back(field);

  FakeFormFetcher fetcher;
  fetcher.Fetch();
  PasswordFormManager form_manager(password_manager(), client(),
                                   client()->driver(), *observed_form(),
                                   std::make_unique<MockFormSaver>(), &fetcher);
  form_manager.Init(nullptr);
  fetcher.SetNonFederated({saved_match()}, 0u);

  // User submits current and new credentials to the observed form.
  PasswordForm credentials(*observed_form());
  credentials.username_element.clear();
  credentials.password_value = saved_match()->password_value;
  credentials.new_password_value = ASCIIToUTF16("test2");
  credentials.preferred = true;
  form_manager.ProvisionallySave(credentials);

  // Successful login. The PasswordManager would instruct PasswordFormManager
  // to save, and since this is an update, it should know not to save as a new
  // login.
  EXPECT_FALSE(form_manager.IsNewLogin());
  EXPECT_FALSE(form_manager.IsPossibleChangePasswordFormWithoutUsername());

  // By now, the PasswordFormManager should have promoted the new password value
  // already to be the current password, and should no longer maintain any info
  // about the new password value.
  EXPECT_EQ(credentials.new_password_value,
            form_manager.GetPendingCredentials().password_value);
  EXPECT_TRUE(form_manager.GetPendingCredentials().new_password_value.empty());

  // Trigger saving to exercise some special case handling during updating.
  PasswordForm new_credentials;
  EXPECT_CALL(MockFormSaver::Get(&form_manager), Update(_, _, _, nullptr))
      .WillOnce(SaveArg<0>(&new_credentials));

  form_manager.Update(*saved_match());

  // The password is updated.
  EXPECT_EQ(credentials.new_password_value, new_credentials.password_value);
  EXPECT_EQ(saved_match()->username_element, new_credentials.username_element);
  EXPECT_EQ(saved_match()->password_element, new_credentials.password_element);
  EXPECT_EQ(saved_match()->submit_element, new_credentials.submit_element);
}

TEST_F(PasswordFormManagerTest, TestUpdateNoUsernameTextfieldPresent) {
  // Add a new password field to the test form and insert a |username_value|
  // unlikely to be a real username. The PasswordFormManager should still save
  // the password from this field, instead of the current password field.
  observed_form()->new_password_element = ASCIIToUTF16("NewPasswd");
  autofill::FormFieldData field;
  field.label = ASCIIToUTF16("NewPasswd");
  field.name = ASCIIToUTF16("NewPasswd");
  field.form_control_type = "password";
  observed_form()->form_data.fields.push_back(field);

  FakeFormFetcher fetcher;
  fetcher.Fetch();
  PasswordFormManager form_manager(password_manager(), client(),
                                   client()->driver(), *observed_form(),
                                   std::make_unique<MockFormSaver>(), &fetcher);
  form_manager.Init(nullptr);
  fetcher.SetNonFederated({saved_match()}, 0u);

  // User submits current and new credentials to the observed form.
  PasswordForm credentials(*observed_form());
  // The |username_value| contains a text that's unlikely to be real username.
  credentials.username_value = ASCIIToUTF16("3");
  credentials.password_value = saved_match()->password_value;
  credentials.new_password_value = ASCIIToUTF16("test2");
  credentials.preferred = true;
  form_manager.ProvisionallySave(credentials);

  // Successful login. The PasswordManager would instruct PasswordFormManager
  // to save, and since this is an update, it should know not to save as a new
  // login.
  EXPECT_FALSE(form_manager.IsNewLogin());
  EXPECT_TRUE(form_manager.IsPossibleChangePasswordFormWithoutUsername());

  // By now, the PasswordFormManager should have promoted the new password value
  // already to be the current password, and should no longer maintain any info
  // about the new password value.
  EXPECT_EQ(saved_match()->username_value,
            form_manager.GetPendingCredentials().username_value);
  EXPECT_EQ(credentials.new_password_value,
            form_manager.GetPendingCredentials().password_value);
  EXPECT_TRUE(form_manager.GetPendingCredentials().new_password_value.empty());

  // Trigger saving to exercise some special case handling during updating.
  PasswordForm new_credentials;
  EXPECT_CALL(MockFormSaver::Get(&form_manager), Update(_, _, _, nullptr))
      .WillOnce(SaveArg<0>(&new_credentials));

  form_manager.Update(form_manager.GetPendingCredentials());

  // The password should be updated, but the username should not.
  EXPECT_EQ(saved_match()->username_value, new_credentials.username_value);
  EXPECT_EQ(credentials.new_password_value, new_credentials.password_value);
  EXPECT_EQ(saved_match()->username_element, new_credentials.username_element);
  EXPECT_EQ(saved_match()->password_element, new_credentials.password_element);
  EXPECT_TRUE(new_credentials.new_password_value.empty());
  EXPECT_TRUE(new_credentials.new_password_element.empty());
  EXPECT_EQ(saved_match()->submit_element, new_credentials.submit_element);
}

// Test the case when a user changes the username to the value of another field
// from the form.
TEST_F(PasswordFormManagerTest, UpdateUsername_ValueOfAnotherField) {
  for (bool captured_username_is_empty : {false, true}) {
    SCOPED_TRACE(testing::Message() << "captured_username_is_empty="
                                    << captured_username_is_empty);
    PasswordForm observed(*observed_form());
    // Set |FormData| to upload a username vote.
    autofill::FormFieldData field;
    field.name = ASCIIToUTF16("full_name");
    field.form_control_type = "text";
    observed.form_data.fields.push_back(field);
    field.name = ASCIIToUTF16("correct_username_element");
    field.form_control_type = "text";
    observed.form_data.fields.push_back(field);
    field.name = ASCIIToUTF16("Passwd");
    field.form_control_type = "password";
    observed.form_data.fields.push_back(field);

    PasswordFormManager form_manager(
        password_manager(), client(), client()->driver(), observed,
        std::make_unique<NiceMock<MockFormSaver>>(), fake_form_fetcher());
    form_manager.Init(nullptr);
    fake_form_fetcher()->SetNonFederated(std::vector<const PasswordForm*>(),
                                         0u);

    // User enters credential in the form.
    PasswordForm credential(observed);
    credential.username_value = captured_username_is_empty
                                    ? base::string16()
                                    : ASCIIToUTF16("typed_username");
    credential.password_value = ASCIIToUTF16("password");
    credential.other_possible_usernames.push_back(
        ValueElementPair(ASCIIToUTF16("edited_username"),
                         ASCIIToUTF16("correct_username_element")));
    form_manager.ProvisionallySave(credential);

    // User edits username in a prompt.
    form_manager.UpdateUsername(ASCIIToUTF16("edited_username"));
    EXPECT_EQ(form_manager.GetPendingCredentials().username_value,
              ASCIIToUTF16("edited_username"));
    EXPECT_EQ(form_manager.GetPendingCredentials().username_element,
              ASCIIToUTF16("correct_username_element"));
    EXPECT_EQ(form_manager.GetPendingCredentials().password_value,
              ASCIIToUTF16("password"));
    EXPECT_TRUE(form_manager.IsNewLogin());

    // User clicks save, the edited username is saved.
    PasswordForm saved_result;
    EXPECT_CALL(MockFormSaver::Get(&form_manager), Save(_, IsEmpty()))
        .WillOnce(SaveArg<0>(&saved_result));
    // Expect a username edited vote.
    FieldTypeMap expected_types;
    expected_types[ASCIIToUTF16("full_name")] = autofill::UNKNOWN_TYPE;
    expected_types[ASCIIToUTF16("correct_username_element")] =
        autofill::USERNAME;
    expected_types[ASCIIToUTF16("Passwd")] = autofill::PASSWORD;
    VoteTypeMap expected_vote_types = {
        {ASCIIToUTF16("correct_username_element"),
         autofill::AutofillUploadContents::Field::USERNAME_EDITED}};
    EXPECT_CALL(
        *client()->mock_driver()->mock_autofill_download_manager(),
        StartUploadRequest(
            AllOf(SignatureIsSameAs(observed),
                  UploadedAutofillTypesAre(expected_types),
                  HasGenerationVote(false), VoteTypesAre(expected_vote_types)),
            _, Contains(autofill::USERNAME), _, _, nullptr));
    form_manager.Save();

    // Check what is saved.
    EXPECT_EQ(ASCIIToUTF16("edited_username"), saved_result.username_value);
    EXPECT_EQ(ASCIIToUTF16("correct_username_element"),
              saved_result.username_element);
    EXPECT_EQ(ASCIIToUTF16("password"), saved_result.password_value);
    if (captured_username_is_empty) {
      EXPECT_TRUE(saved_result.other_possible_usernames.empty());
    } else {
      EXPECT_THAT(
          saved_result.other_possible_usernames,
          ElementsAre(ValueElementPair(ASCIIToUTF16("typed_username"),
                                       observed_form()->username_element)));
    }
  }
}

// Test the case when a user updates the username to an already existing one.
TEST_F(PasswordFormManagerTest, UpdateUsername_ValueSavedInStore) {
  for (bool captured_username_is_empty : {false, true}) {
    SCOPED_TRACE(testing::Message() << "captured_username_is_empty="
                                    << captured_username_is_empty);
    // We have an already existing credential.
    fake_form_fetcher()->SetNonFederated({saved_match()}, 0u);

    // User enters credential in the form.
    PasswordForm credential(*observed_form());
    credential.username_value = captured_username_is_empty
                                    ? base::string16()
                                    : ASCIIToUTF16("different_username");
    credential.password_value = ASCIIToUTF16("different_pass");
    credential.preferred = true;
    form_manager()->ProvisionallySave(credential);

    // User edits username in a prompt to one already existing.
    form_manager()->UpdateUsername(saved_match()->username_value);

    // The username in credentials is expected to be updated.
    EXPECT_EQ(saved_match()->username_value,
              form_manager()->GetPendingCredentials().username_value);
    EXPECT_EQ(saved_match()->username_element,
              form_manager()->GetPendingCredentials().username_element);
    EXPECT_EQ(ASCIIToUTF16("different_pass"),
              form_manager()->GetPendingCredentials().password_value);
    EXPECT_FALSE(form_manager()->IsNewLogin());

    // Create the expected credential to be saved.
    PasswordForm expected_pending(credential);
    expected_pending.origin = saved_match()->origin;
    expected_pending.form_data = saved_match()->form_data;
    expected_pending.times_used = 1;
    expected_pending.username_value = saved_match()->username_value;

    // User clicks save, edited username is saved, password updated.
    EXPECT_CALL(MockFormSaver::Get(form_manager()),
                Update(expected_pending,
                       ElementsAre(Pair(saved_match()->username_value,
                                        Pointee(*saved_match()))),
                       Pointee(IsEmpty()), nullptr));
    // Expect a username reusing vote.
    FieldTypeMap expected_types;
    expected_types[ASCIIToUTF16("full_name")] = autofill::UNKNOWN_TYPE;
    expected_types[expected_pending.username_element] = autofill::USERNAME;
    expected_types[expected_pending.password_element] =
        autofill::ACCOUNT_CREATION_PASSWORD;
    VoteTypeMap expected_vote_types = {
        {expected_pending.username_element,
         autofill::AutofillUploadContents::Field::CREDENTIALS_REUSED}};
    EXPECT_CALL(
        *client()->mock_driver()->mock_autofill_download_manager(),
        StartUploadRequest(
            AllOf(SignatureIsSameAs(expected_pending),
                  UploadedAutofillTypesAre(expected_types),
                  HasGenerationVote(false), VoteTypesAre(expected_vote_types)),
            _, Contains(autofill::USERNAME), _, _, nullptr));
    form_manager()->Save();
  }
}

// Tests the case when the username value edited in prompt doesn't coincides
// neither with other values on the form nor with saved usernames.
TEST_F(PasswordFormManagerTest, UpdateUsername_NoMatchNeitherOnFormNorInStore) {
  for (bool captured_username_is_empty : {false, true}) {
    SCOPED_TRACE(testing::Message() << "captured_username_is_empty="
                                    << captured_username_is_empty);
    PasswordForm observed(*observed_form());
    // Assign any |FormData| to allow crowdsourcing of this form.
    observed.form_data = saved_match()->form_data;
    PasswordFormManager form_manager(
        password_manager(), client(), client()->driver(), observed,
        std::make_unique<NiceMock<MockFormSaver>>(), fake_form_fetcher());
    form_manager.Init(nullptr);
    // We have an already existing credential.
    fake_form_fetcher()->SetNonFederated({saved_match()}, 0u);

    // A user enters a credential in the form.
    PasswordForm credential(observed);
    credential.username_value = captured_username_is_empty
                                    ? base::string16()
                                    : ASCIIToUTF16("captured_username");
    credential.password_value = ASCIIToUTF16("different_pass");
    credential.preferred = true;
    form_manager.ProvisionallySave(credential);

    // User edits username. The username doesn't exist neither in the store nor
    // on the form.
    form_manager.UpdateUsername(ASCIIToUTF16("new_username"));

    // As there is no match on the form, |username_element| is empty.
    EXPECT_TRUE(form_manager.GetPendingCredentials().username_element.empty());
    EXPECT_EQ(ASCIIToUTF16("new_username"),
              form_manager.GetPendingCredentials().username_value);
    EXPECT_EQ(ASCIIToUTF16("different_pass"),
              form_manager.GetPendingCredentials().password_value);
    EXPECT_TRUE(form_manager.IsNewLogin());

    PasswordForm expected_pending(credential);
    expected_pending.username_value = ASCIIToUTF16("new_username");
    expected_pending.username_element = base::string16();
    if (!captured_username_is_empty) {
      // A non-empty captured username value should be saved to recover later if
      // a user makes a mistake in username editing.
      expected_pending.other_possible_usernames.push_back(ValueElementPair(
          ASCIIToUTF16("captured_username"), ASCIIToUTF16("Email")));
    }

    // User clicks save, the edited username is saved, the password is updated,
    // no username vote is uploaded.
    PasswordForm actual_saved_form;
    EXPECT_CALL(MockFormSaver::Get(&form_manager),
                Save(_, ElementsAre(Pair(saved_match()->username_value,
                                         Pointee(*saved_match())))))
        .WillOnce(SaveArg<0>(&actual_saved_form));
    EXPECT_CALL(*client()->mock_driver()->mock_autofill_download_manager(),
                StartUploadRequest(_, _, Not(Contains(autofill::USERNAME)), _,
                                   _, nullptr));
    form_manager.Save();

    // Can't verify |date_created |, so ignore it in form comparison.
    actual_saved_form.date_created = expected_pending.date_created;
    EXPECT_EQ(expected_pending, actual_saved_form);
  }
}

// Tests the case when a user clears the username value in a prompt.
TEST_F(PasswordFormManagerTest, UpdateUsername_UserRemovedUsername) {
  PasswordForm observed(*observed_form());
  // Assign any |FormData| to allow crowdsourcing of this form.
  observed.form_data = saved_match()->form_data;
  PasswordFormManager form_manager(
      password_manager(), client(), client()->driver(), observed,
      std::make_unique<NiceMock<MockFormSaver>>(), fake_form_fetcher());
  form_manager.Init(nullptr);
  fake_form_fetcher()->SetNonFederated(std::vector<const PasswordForm*>(), 0u);

  // The user enters credential in the form.
  PasswordForm credential(observed);
  credential.username_value = ASCIIToUTF16("pin_code");
  credential.password_value = ASCIIToUTF16("password");
  credential.other_possible_usernames.push_back(
      ValueElementPair(base::string16(), ASCIIToUTF16("empty_field")));
  form_manager.ProvisionallySave(credential);

  // The user clears the username value in the prompt.
  form_manager.UpdateUsername(base::string16());
  EXPECT_TRUE(form_manager.GetPendingCredentials().username_value.empty());
  EXPECT_TRUE(form_manager.GetPendingCredentials().username_element.empty());
  EXPECT_EQ(form_manager.GetPendingCredentials().password_value,
            ASCIIToUTF16("password"));
  EXPECT_TRUE(form_manager.IsNewLogin());

  // The user clicks save, empty username is saved.
  PasswordForm saved_result;
  EXPECT_CALL(MockFormSaver::Get(&form_manager), Save(_, IsEmpty()))
      .WillOnce(SaveArg<0>(&saved_result));
  EXPECT_CALL(*client()->mock_driver()->mock_autofill_download_manager(),
              StartUploadRequest(_, _, Not(Contains(autofill::USERNAME)), _, _,
                                 nullptr));
  form_manager.Save();

  EXPECT_TRUE(saved_result.username_value.empty());
  EXPECT_TRUE(saved_result.username_element.empty());
  EXPECT_EQ(ASCIIToUTF16("password"), saved_result.password_value);
  EXPECT_THAT(saved_result.other_possible_usernames,
              ElementsAre(ValueElementPair(ASCIIToUTF16("pin_code"),
                                           observed_form()->username_element)));
}

// Test that when user updates username to a PSL matching credential, we should
// handle it as a new login.
TEST_F(PasswordFormManagerTest, UpdateUsername_PslMatch) {
  fake_form_fetcher()->SetNonFederated({psl_saved_match()}, 0u);
  // The user submits a credential of the observed form.
  PasswordForm credential(*observed_form());
  credential.username_value = ASCIIToUTF16("some_username");
  credential.password_value = ASCIIToUTF16("some_pass");
  form_manager()->ProvisionallySave(credential);

  // The user edits the username to match the PSL entry.
  form_manager()->UpdateUsername(psl_saved_match()->username_value);
  EXPECT_EQ(psl_saved_match()->username_value,
            form_manager()->GetPendingCredentials().username_value);
  EXPECT_TRUE(form_manager()->IsNewLogin());
  EXPECT_EQ(ASCIIToUTF16("some_pass"),
            form_manager()->GetPendingCredentials().password_value);

  // The user clicks save, the edited username is saved.
  PasswordForm saved_result;
  EXPECT_CALL(MockFormSaver::Get(form_manager()),
              Save(_, ElementsAre(Pair(psl_saved_match()->username_value,
                                       Pointee(*psl_saved_match())))))
      .WillOnce(SaveArg<0>(&saved_result));
  // As the credential is re-used successfully, expect a username vote.
  EXPECT_CALL(
      *client()->mock_driver()->mock_autofill_download_manager(),
      StartUploadRequest(_, _, Contains(autofill::USERNAME), _, _, nullptr));
  form_manager()->Save();

  // Check what is saved.
  EXPECT_EQ(psl_saved_match()->username_value, saved_result.username_value);
  EXPECT_EQ(ASCIIToUTF16("some_pass"), saved_result.password_value);
}

// Test that when user selects a password in the bubble, the pending credentials
// are updated accordingly.
TEST_F(PasswordFormManagerTest, TestSelectPasswordMethod) {
  for (bool has_password : {true, false}) {
    for (bool has_new_password : {true, false}) {
      for (bool has_passwords_revealed : {true, false}) {
        if (!has_password && !has_new_password)
          continue;
        SCOPED_TRACE(testing::Message() << "has_password=" << has_password);
        SCOPED_TRACE(testing::Message()
                     << "has_new_password=" << has_new_password);
        SCOPED_TRACE(testing::Message()
                     << "has_passwords_revealed=" << has_passwords_revealed);

        PasswordForm observed(*observed_form());
        // Observe two password fields, between which we must select.
        autofill::FormFieldData field;
        field.name = ASCIIToUTF16("correct_password_element");
        field.form_control_type = "password";
        observed.form_data.fields.push_back(field);
        field.name = ASCIIToUTF16("other_password_element");
        field.form_control_type = "password";
        observed.form_data.fields.push_back(field);

        PasswordFormManager form_manager(
            password_manager(), client(), client()->driver(), observed,
            std::make_unique<NiceMock<MockFormSaver>>(), fake_form_fetcher());
        form_manager.Init(nullptr);
        fake_form_fetcher()->SetNonFederated(std::vector<const PasswordForm*>(),
                                             0u);

        // User enters credential in the form. We autodetect the wrong password
        // field.
        PasswordForm credential(observed);
        if (has_password) {
          credential.password_value = ASCIIToUTF16("not-a-password");
          credential.password_element = ASCIIToUTF16("other_password_element");
        }
        if (has_new_password) {
          credential.new_password_value = ASCIIToUTF16("not-a-password");
          credential.new_password_element =
              ASCIIToUTF16("other_password_element");
        }
        credential.all_possible_passwords = {
            {ASCIIToUTF16("p4ssword"),
             ASCIIToUTF16("correct_password_element")},
            {ASCIIToUTF16("not-a-password"),
             ASCIIToUTF16("other_password_element")}};
        form_manager.ProvisionallySave(credential);

        // Pending credentials have the wrong values.
        EXPECT_EQ(form_manager.GetPendingCredentials().password_value,
                  ASCIIToUTF16("not-a-password"));
        EXPECT_EQ(form_manager.GetPendingCredentials().password_element,
                  ASCIIToUTF16("other_password_element"));
        // User selects another password in a prompt.
        if (has_passwords_revealed)
          form_manager.OnPasswordsRevealed();
        form_manager.UpdatePasswordValue(ASCIIToUTF16("p4ssword"));
        // Pending credentials are also corrected.
        EXPECT_EQ(form_manager.GetPendingCredentials().password_value,
                  ASCIIToUTF16("p4ssword"));
        EXPECT_EQ(form_manager.GetPendingCredentials().password_element,
                  ASCIIToUTF16("correct_password_element"));
        EXPECT_TRUE(form_manager.IsNewLogin());

        // User clicks save, selected password is saved.
        PasswordForm saved_result;
        EXPECT_CALL(MockFormSaver::Get(&form_manager), Save(_, IsEmpty()))
            .WillOnce(SaveArg<0>(&saved_result));
        // Expect a password edited vote.
        FieldTypeMap expected_types;
        expected_types[ASCIIToUTF16("correct_password_element")] =
            autofill::PASSWORD;
        expected_types[ASCIIToUTF16("other_password_element")] =
            autofill::UNKNOWN_TYPE;
        EXPECT_CALL(*client()->mock_driver()->mock_autofill_download_manager(),
                    StartUploadRequest(
                        AllOf(SignatureIsSameAs(observed),
                              UploadedAutofillTypesAre(expected_types),
                              HasGenerationVote(false),
                              PasswordsWereRevealed(has_passwords_revealed)),
                        _, Contains(autofill::PASSWORD), _, _, nullptr));
        form_manager.Save();

        EXPECT_EQ(ASCIIToUTF16("p4ssword"), saved_result.password_value);
        EXPECT_EQ(ASCIIToUTF16("correct_password_element"),
                  saved_result.password_element);
        EXPECT_EQ(base::string16(), saved_result.new_password_value);
        EXPECT_EQ(base::string16(), saved_result.new_password_element);
      }
    }
  }
}

TEST_F(PasswordFormManagerTest, GenerationStatusChangedWithPassword) {
  PasswordForm generated_form = *observed_form();
  generated_form.type = PasswordForm::TYPE_GENERATED;
  generated_form.username_value = ASCIIToUTF16("username");
  generated_form.password_value = ASCIIToUTF16("password2");
  generated_form.preferred = true;

  PasswordForm submitted_form(generated_form);
  submitted_form.password_value = ASCIIToUTF16("password3");

  fake_form_fetcher()->SetNonFederated({&generated_form}, 0u);

  form_manager()->ProvisionallySave(submitted_form);

  PasswordForm new_credentials;
  EXPECT_CALL(MockFormSaver::Get(form_manager()), Update(_, _, _, nullptr))
      .WillOnce(testing::SaveArg<0>(&new_credentials));
  form_manager()->Save();

  EXPECT_EQ(PasswordForm::TYPE_MANUAL, new_credentials.type);
}

TEST_F(PasswordFormManagerTest, GenerationStatusNotUpdatedIfPasswordUnchanged) {
  base::HistogramTester histogram_tester;

  PasswordForm generated_form = *observed_form();
  generated_form.type = PasswordForm::TYPE_GENERATED;
  generated_form.username_value = ASCIIToUTF16("username");
  generated_form.password_value = ASCIIToUTF16("password2");
  generated_form.preferred = true;

  PasswordForm submitted_form(generated_form);

  fake_form_fetcher()->SetNonFederated({&generated_form}, 0u);

  form_manager()->ProvisionallySave(submitted_form);

  PasswordForm new_credentials;
  EXPECT_CALL(MockFormSaver::Get(form_manager()), Update(_, _, _, nullptr))
      .WillOnce(testing::SaveArg<0>(&new_credentials));
  form_manager()->Save();

  EXPECT_EQ(PasswordForm::TYPE_GENERATED, new_credentials.type);
  histogram_tester.ExpectBucketCount("PasswordGeneration.SubmissionEvent",
                                     metrics_util::PASSWORD_USED, 1);

  // On the second reuse, the metric is not reported.
  generated_form.times_used = 1;
  fake_form_fetcher()->SetNonFederated({&generated_form}, 0u);
  form_manager()->ProvisionallySave(submitted_form);
  EXPECT_CALL(MockFormSaver::Get(form_manager()), Update(_, _, _, _));
  form_manager()->Save();
  histogram_tester.ExpectBucketCount("PasswordGeneration.SubmissionEvent",
                                     metrics_util::PASSWORD_USED, 1);
}

TEST_F(PasswordFormManagerTest, GeneratedPasswordIsOverridden) {
  base::HistogramTester histogram_tester;

  PasswordForm generated_form = *observed_form();
  generated_form.type = PasswordForm::TYPE_GENERATED;
  generated_form.username_value = ASCIIToUTF16("username");
  generated_form.password_value = ASCIIToUTF16("password2");
  generated_form.preferred = true;

  PasswordForm submitted_form(generated_form);
  submitted_form.password_value = ASCIIToUTF16("another_password");

  fake_form_fetcher()->SetNonFederated({&generated_form}, 0u);

  form_manager()->ProvisionallySave(submitted_form);

  PasswordForm new_credentials;
  EXPECT_CALL(MockFormSaver::Get(form_manager()), Update(_, _, _, nullptr))
      .WillOnce(testing::SaveArg<0>(&new_credentials));
  form_manager()->Save();

  EXPECT_EQ(PasswordForm::TYPE_MANUAL, new_credentials.type);
  EXPECT_EQ(ASCIIToUTF16("another_password"), new_credentials.password_value);
  histogram_tester.ExpectBucketCount("PasswordGeneration.SubmissionEvent",
                                     metrics_util::PASSWORD_OVERRIDDEN, 1);

  // On the reuse of the overriden password, the metric is not reported.
  fake_form_fetcher()->SetNonFederated({&new_credentials}, 0u);
  form_manager()->ProvisionallySave(new_credentials);
  EXPECT_CALL(MockFormSaver::Get(form_manager()), Update(_, _, _, _));
  form_manager()->Save();
  histogram_tester.ExpectBucketCount("PasswordGeneration.SubmissionEvent",
                                     metrics_util::PASSWORD_OVERRIDDEN, 1);
  histogram_tester.ExpectBucketCount("PasswordGeneration.SubmissionEvent",
                                     metrics_util::PASSWORD_USED, 0);
}

// Test that ProcessFrame is called on receiving matches from the fetcher,
// resulting in a FillPasswordForm call.
TEST_F(PasswordFormManagerTest, ProcessFrame) {
  EXPECT_CALL(*client()->mock_driver(), FillPasswordForm(_));
  fake_form_fetcher()->SetNonFederated({saved_match()}, 0u);
}

// Test that driver is informed when there are not saved credentials.
TEST_F(PasswordFormManagerTest, InformNoSavedCredentials) {
  EXPECT_CALL(*client()->mock_driver(), InformNoSavedCredentials());
  fake_form_fetcher()->SetNonFederated({}, 0u);
}

// Test that ProcessFrame can also be called directly, resulting in an
// additional FillPasswordForm call.
TEST_F(PasswordFormManagerTest, ProcessFrame_MoreProcessFrameMoreFill) {
  EXPECT_CALL(*client()->mock_driver(), FillPasswordForm(_)).Times(2);
  fake_form_fetcher()->SetNonFederated({saved_match()}, 0u);
  form_manager()->ProcessFrame(client()->mock_driver()->AsWeakPtr());
}

// Test that Chrome stops autofilling if triggered too many times.
TEST_F(PasswordFormManagerTest, ProcessFrame_MaxTimes) {
  constexpr int kMaxAutofills = PasswordFormManager::kMaxTimesAutofill;
  constexpr int kExtraProcessRequests = 3;
  // Expect one call for each ProcessFrame() and one for SetNonFederated().
  EXPECT_CALL(*client()->mock_driver(), FillPasswordForm(_))
      .Times(kMaxAutofills + 1);
  fake_form_fetcher()->SetNonFederated({saved_match()}, 0u);
  // Process more times to exceed the limit.
  for (int i = 0; i < kMaxAutofills + kExtraProcessRequests; i++) {
    form_manager()->ProcessFrame(client()->mock_driver()->AsWeakPtr());
  }
}

// Test that when ProcessFrame is called on a driver added after receiving
// matches, such driver is still told to call FillPasswordForm.
TEST_F(PasswordFormManagerTest, ProcessFrame_TwoDrivers) {
  NiceMock<MockPasswordManagerDriver> second_driver;

  EXPECT_CALL(*client()->mock_driver(), FillPasswordForm(_));
  EXPECT_CALL(second_driver, FillPasswordForm(_));
  fake_form_fetcher()->SetNonFederated({saved_match()}, 0u);
  form_manager()->ProcessFrame(second_driver.AsWeakPtr());
}

// Test that a second driver is added before receiving matches, such driver is
// also told to call FillPasswordForm once the matches arrive.
TEST_F(PasswordFormManagerTest, ProcessFrame_DriverBeforeMatching) {
  NiceMock<MockPasswordManagerDriver> second_driver;
  form_manager()->ProcessFrame(second_driver.AsWeakPtr());

  EXPECT_CALL(*client()->mock_driver(), FillPasswordForm(_));
  EXPECT_CALL(second_driver, FillPasswordForm(_));
  fake_form_fetcher()->SetNonFederated({saved_match()}, 0u);
}

// Test that if the fetcher updates the information about stored matches,
// autofill is re-triggered.
TEST_F(PasswordFormManagerTest, ProcessFrame_StoreUpdatesCausesAutofill) {
  EXPECT_CALL(*client()->mock_driver(), FillPasswordForm(_)).Times(2);
  std::vector<const PasswordForm*> matches = {saved_match()};
  fake_form_fetcher()->SetNonFederated(matches, 0u);
  fake_form_fetcher()->Fetch();
  fake_form_fetcher()->SetNonFederated(matches, 0u);
}

// TODO(crbug.com/639786): Restore the following test:
// when PasswordFormManager::Save or PasswordFormManager::Update is called, then
// PasswordFormManager also calls PasswordManager::UpdateFormManagers.

TEST_F(PasswordFormManagerTest, UploadChangePasswordForm) {
  autofill::ServerFieldType kChangePasswordVotes[] = {
      autofill::NEW_PASSWORD, autofill::PROBABLY_NEW_PASSWORD,
      autofill::NOT_NEW_PASSWORD};
  bool kFalseTrue[] = {false, true};
  for (autofill::ServerFieldType vote : kChangePasswordVotes) {
    for (bool has_confirmation_field : kFalseTrue)
      ChangePasswordUploadTest(vote, has_confirmation_field);
  }
}

TEST_F(PasswordFormManagerTest, TestUpdatePSLMatchedCredentials) {
  FakeFormFetcher fetcher;
  fetcher.Fetch();
  PasswordFormManager form_manager(password_manager(), client(),
                                   client()->driver(), *observed_form(),
                                   std::make_unique<MockFormSaver>(), &fetcher);
  form_manager.Init(nullptr);
  fetcher.SetNonFederated({saved_match(), psl_saved_match()}, 0u);

  // User submits a credentials with an old username and a new password.
  PasswordForm credentials(*observed_form());
  credentials.username_value = saved_match()->username_value;
  credentials.password_value = ASCIIToUTF16("new_password");
  credentials.preferred = true;
  form_manager.ProvisionallySave(credentials);

  // Successful login. The PasswordManager would instruct PasswordFormManager
  // to save, and since this is an update, it should know not to save as a new
  // login.
  EXPECT_FALSE(form_manager.IsNewLogin());

  // Trigger saving to exercise some special case handling during updating.
  PasswordForm new_credentials;
  std::vector<autofill::PasswordForm> credentials_to_update;
  EXPECT_CALL(MockFormSaver::Get(&form_manager), Update(_, _, _, nullptr))
      .WillOnce(testing::DoAll(SaveArg<0>(&new_credentials),
                               SaveArgPointee<2>(&credentials_to_update)));
  EXPECT_CALL(*client()->mock_driver()->mock_autofill_download_manager(),
              StartUploadRequest(_, false, _, _, true, nullptr));

  form_manager.Save();

  // No meta-information should be updated, only the password.
  EXPECT_EQ(credentials.password_value, new_credentials.password_value);
  EXPECT_EQ(saved_match()->username_value, new_credentials.username_value);
  EXPECT_EQ(saved_match()->username_element, new_credentials.username_element);
  EXPECT_EQ(saved_match()->password_element, new_credentials.password_element);
  EXPECT_EQ(saved_match()->origin, new_credentials.origin);

  ASSERT_EQ(1u, credentials_to_update.size());
  EXPECT_EQ(credentials.password_value,
            credentials_to_update[0].password_value);
  EXPECT_EQ(psl_saved_match()->username_element,
            credentials_to_update[0].username_element);
  EXPECT_EQ(psl_saved_match()->username_element,
            credentials_to_update[0].username_element);
  EXPECT_EQ(psl_saved_match()->password_element,
            credentials_to_update[0].password_element);
  EXPECT_EQ(psl_saved_match()->origin, credentials_to_update[0].origin);
}

TEST_F(PasswordFormManagerTest,
       TestNotUpdatePSLMatchedCredentialsWithAnotherUsername) {
  FakeFormFetcher fetcher;
  fetcher.Fetch();
  PasswordFormManager form_manager(password_manager(), client(),
                                   client()->driver(), *observed_form(),
                                   std::make_unique<MockFormSaver>(), &fetcher);
  form_manager.Init(nullptr);
  psl_saved_match()->username_value += ASCIIToUTF16("1");
  fetcher.SetNonFederated({saved_match(), psl_saved_match()}, 0u);

  // User submits a credentials with an old username and a new password.
  PasswordForm credentials(*observed_form());
  credentials.username_value = saved_match()->username_value;
  credentials.password_value = ASCIIToUTF16("new_password");
  credentials.preferred = true;
  form_manager.ProvisionallySave(credentials);

  // Successful login. The PasswordManager would instruct PasswordFormManager
  // to save, and since this is an update, it should know not to save as a new
  // login.
  EXPECT_FALSE(form_manager.IsNewLogin());

  // Trigger saving to exercise some special case handling during updating.
  PasswordForm new_credentials;
  EXPECT_CALL(MockFormSaver::Get(&form_manager),
              Update(_, _, Pointee(IsEmpty()), nullptr))
      .WillOnce(testing::SaveArg<0>(&new_credentials));
  EXPECT_CALL(*client()->mock_driver()->mock_autofill_download_manager(),
              StartUploadRequest(_, false, _, _, true, nullptr));

  form_manager.Save();

  // No meta-information should be updated, only the password.
  EXPECT_EQ(credentials.password_value, new_credentials.password_value);
  EXPECT_EQ(saved_match()->username_value, new_credentials.username_value);
  EXPECT_EQ(saved_match()->password_element, new_credentials.password_element);
  EXPECT_EQ(saved_match()->username_element, new_credentials.username_element);
  EXPECT_EQ(saved_match()->origin, new_credentials.origin);
}

TEST_F(PasswordFormManagerTest,
       TestNotUpdatePSLMatchedCredentialsWithAnotherPassword) {
  FakeFormFetcher fetcher;
  fetcher.Fetch();
  PasswordFormManager form_manager(password_manager(), client(),
                                   client()->driver(), *observed_form(),
                                   std::make_unique<MockFormSaver>(), &fetcher);
  form_manager.Init(nullptr);
  psl_saved_match()->password_value += ASCIIToUTF16("1");
  fetcher.SetNonFederated({saved_match(), psl_saved_match()}, 0u);

  // User submits a credentials with an old username and a new password.
  PasswordForm credentials(*observed_form());
  credentials.username_value = saved_match()->username_value;
  credentials.password_value = ASCIIToUTF16("new_password");
  credentials.preferred = true;
  form_manager.ProvisionallySave(credentials);

  // Successful login. The PasswordManager would instruct PasswordFormManager
  // to save, and since this is an update, it should know not to save as a new
  // login.
  EXPECT_FALSE(form_manager.IsNewLogin());

  // Trigger saving to exercise some special case handling during updating.
  PasswordForm new_credentials;
  EXPECT_CALL(MockFormSaver::Get(&form_manager),
              Update(_, _, Pointee(IsEmpty()), nullptr))
      .WillOnce(testing::SaveArg<0>(&new_credentials));
  EXPECT_CALL(*client()->mock_driver()->mock_autofill_download_manager(),
              StartUploadRequest(_, false, _, _, true, nullptr));

  form_manager.Save();

  // No meta-information should be updated, only the password.
  EXPECT_EQ(credentials.password_value, new_credentials.password_value);
  EXPECT_EQ(saved_match()->username_value, new_credentials.username_value);
  EXPECT_EQ(saved_match()->password_element, new_credentials.password_element);
  EXPECT_EQ(saved_match()->username_element, new_credentials.username_element);
  EXPECT_EQ(saved_match()->origin, new_credentials.origin);
}

TEST_F(PasswordFormManagerTest, TestNotUpdateWhenOnlyPslMatched) {
  FakeFormFetcher fetcher;
  fetcher.Fetch();
  PasswordFormManager form_manager(password_manager(), client(),
                                   client()->driver(), *observed_form(),
                                   std::make_unique<MockFormSaver>(), &fetcher);
  form_manager.Init(nullptr);
  fetcher.SetNonFederated({psl_saved_match()}, 0u);

  // User submits a credentials with an old username and a new password.
  PasswordForm credentials(*observed_form());
  credentials.username_value = saved_match()->username_value;
  credentials.password_value = ASCIIToUTF16("new_password");
  credentials.preferred = true;
  form_manager.ProvisionallySave(credentials);

  EXPECT_TRUE(form_manager.IsNewLogin());

  // PSL matched credential should not be updated, since we are not sure that
  // this is the same credential as submitted one.
  PasswordForm new_credentials;
  EXPECT_CALL(MockFormSaver::Get(&form_manager), Save(_, _))
      .WillOnce(testing::SaveArg<0>(&new_credentials));
  // As the username is re-used, expect a username vote.
  EXPECT_CALL(
      *client()->mock_driver()->mock_autofill_download_manager(),
      StartUploadRequest(_, _, Contains(autofill::USERNAME), _, _, nullptr));
  form_manager.Save();

  EXPECT_EQ(credentials.password_value, new_credentials.password_value);
  EXPECT_EQ(credentials.username_value, new_credentials.username_value);
  EXPECT_EQ(credentials.password_element, new_credentials.password_element);
  EXPECT_EQ(credentials.username_element, new_credentials.username_element);
  EXPECT_EQ(credentials.origin, new_credentials.origin);
}

TEST_F(PasswordFormManagerTest,
       TestSavingOnChangePasswordFormGenerationNoStoredForms) {
  fake_form_fetcher()->SetNonFederated(std::vector<const PasswordForm*>(), 0u);

  // User submits change password form and there is no stored credentials.
  PasswordForm credentials = *observed_form();
  credentials.username_element.clear();
  credentials.password_value = saved_match()->password_value;
  credentials.new_password_element = ASCIIToUTF16("NewPasswd");
  credentials.new_password_value = ASCIIToUTF16("new_password");
  credentials.preferred = true;
  form_manager()->PresaveGeneratedPassword(credentials);
  form_manager()->ProvisionallySave(credentials);

  // Successful login. The PasswordManager would instruct PasswordFormManager
  // to save, which should know this is a new login.
  EXPECT_TRUE(form_manager()->IsNewLogin());
  // Make sure the credentials that would be submitted on successful login
  // are going to match submitted form.
  EXPECT_EQ(observed_form()->origin.spec(),
            form_manager()->GetPendingCredentials().origin.spec());
  EXPECT_EQ(observed_form()->signon_realm,
            form_manager()->GetPendingCredentials().signon_realm);
  EXPECT_EQ(observed_form()->action,
            form_manager()->GetPendingCredentials().action);
  EXPECT_TRUE(form_manager()->GetPendingCredentials().preferred);
  EXPECT_EQ(ASCIIToUTF16("new_password"),
            form_manager()->GetPendingCredentials().password_value);
  EXPECT_EQ(base::string16(),
            form_manager()->GetPendingCredentials().username_value);
  EXPECT_TRUE(
      form_manager()->GetPendingCredentials().new_password_element.empty());
  EXPECT_TRUE(
      form_manager()->GetPendingCredentials().new_password_value.empty());
}

TEST_F(PasswordFormManagerTest, TestUpdatingOnChangePasswordFormGeneration) {
  fake_form_fetcher()->SetNonFederated({saved_match()}, 0u);

  // User submits credentials for the change password form, and old password is
  // coincide with password from an existing credentials, so stored credentials
  // should be updated.
  PasswordForm credentials = *observed_form();
  credentials.username_element.clear();
  credentials.password_value = saved_match()->password_value;
  credentials.new_password_element = ASCIIToUTF16("NewPasswd");
  credentials.new_password_value = ASCIIToUTF16("new_password");
  credentials.preferred = true;
  form_manager()->PresaveGeneratedPassword(credentials);
  form_manager()->ProvisionallySave(credentials);

  EXPECT_FALSE(form_manager()->IsNewLogin());
  // Make sure the credentials that would be submitted on successful login
  // are going to match the stored entry in the db.
  EXPECT_EQ(saved_match()->origin.spec(),
            form_manager()->GetPendingCredentials().origin.spec());
  EXPECT_EQ(saved_match()->signon_realm,
            form_manager()->GetPendingCredentials().signon_realm);
  EXPECT_EQ(observed_form()->action,
            form_manager()->GetPendingCredentials().action);
  EXPECT_TRUE(form_manager()->GetPendingCredentials().preferred);
  EXPECT_EQ(ASCIIToUTF16("new_password"),
            form_manager()->GetPendingCredentials().password_value);
  EXPECT_EQ(saved_match()->username_value,
            form_manager()->GetPendingCredentials().username_value);
  EXPECT_TRUE(
      form_manager()->GetPendingCredentials().new_password_element.empty());
  EXPECT_TRUE(
      form_manager()->GetPendingCredentials().new_password_value.empty());
}

TEST_F(PasswordFormManagerTest,
       TestSavingOnChangePasswordFormGenerationNoMatchedForms) {
  fake_form_fetcher()->SetNonFederated({saved_match()}, 0u);

  // User submits credentials for the change password form, and old password is
  // not coincide with password from existing credentials, so new credentials
  // should be saved.
  PasswordForm credentials = *observed_form();
  credentials.username_element.clear();
  credentials.password_value =
      saved_match()->password_value + ASCIIToUTF16("1");
  credentials.new_password_element = ASCIIToUTF16("NewPasswd");
  credentials.new_password_value = ASCIIToUTF16("new_password");
  credentials.preferred = true;
  form_manager()->PresaveGeneratedPassword(credentials);
  form_manager()->ProvisionallySave(credentials);

  EXPECT_TRUE(form_manager()->IsNewLogin());
  // Make sure the credentials that would be submitted on successful login
  // are going to match submitted form.
  EXPECT_EQ(observed_form()->origin.spec(),
            form_manager()->GetPendingCredentials().origin.spec());
  EXPECT_EQ(observed_form()->signon_realm,
            form_manager()->GetPendingCredentials().signon_realm);
  EXPECT_EQ(observed_form()->action,
            form_manager()->GetPendingCredentials().action);
  EXPECT_TRUE(form_manager()->GetPendingCredentials().preferred);
  EXPECT_EQ(ASCIIToUTF16("new_password"),
            form_manager()->GetPendingCredentials().password_value);
  EXPECT_EQ(base::string16(),
            form_manager()->GetPendingCredentials().username_value);
  EXPECT_TRUE(
      form_manager()->GetPendingCredentials().new_password_element.empty());
  EXPECT_TRUE(
      form_manager()->GetPendingCredentials().new_password_value.empty());
}

TEST_F(PasswordFormManagerTest,
       TestUploadVotesForPasswordChangeFormsWithTwoFields) {
  // Turn |observed_form_| into change password form with only 2 fields: an old
  // password and a new password.
  observed_form()->username_element.clear();
  observed_form()->new_password_element = ASCIIToUTF16("NewPasswd");
  autofill::FormFieldData field;
  field.label = ASCIIToUTF16("password");
  field.name = ASCIIToUTF16("Passwd");
  field.form_control_type = "password";
  observed_form()->form_data.fields.push_back(field);

  field.label = ASCIIToUTF16("new password");
  field.name = ASCIIToUTF16("NewPasswd");
  observed_form()->form_data.fields.push_back(field);

  FakeFormFetcher fetcher;
  fetcher.Fetch();
  PasswordFormManager form_manager(password_manager(), client(),
                                   client()->driver(), *observed_form(),
                                   std::make_unique<MockFormSaver>(), &fetcher);
  form_manager.Init(nullptr);
  fetcher.SetNonFederated({saved_match()}, 0u);

  // User submits current and new credentials to the observed form.
  PasswordForm submitted_form(*observed_form());
  submitted_form.password_value = saved_match()->password_value;
  submitted_form.new_password_value = ASCIIToUTF16("test2");
  submitted_form.preferred = true;
  form_manager.ProvisionallySave(submitted_form);

  // Successful login. The PasswordManager would instruct PasswordFormManager
  // to update.
  EXPECT_FALSE(form_manager.IsNewLogin());
  EXPECT_TRUE(form_manager.IsPossibleChangePasswordFormWithoutUsername());

  // By now, the PasswordFormManager should have promoted the new password
  // value already to be the current password, and should no longer maintain
  // any info about the new password value.
  EXPECT_EQ(submitted_form.new_password_value,
            form_manager.GetPendingCredentials().password_value);
  EXPECT_TRUE(form_manager.GetPendingCredentials().new_password_value.empty());

  std::map<base::string16, autofill::ServerFieldType> expected_types;
  expected_types[observed_form()->password_element] = autofill::PASSWORD;
  expected_types[observed_form()->new_password_element] =
      autofill::NEW_PASSWORD;

  autofill::ServerFieldTypeSet expected_available_field_types;
  expected_available_field_types.insert(autofill::PASSWORD);
  expected_available_field_types.insert(autofill::NEW_PASSWORD);

  std::string expected_login_signature =
      autofill::FormStructure(saved_match()->form_data).FormSignatureAsStr();

  // First login vote.
  EXPECT_CALL(*client()->mock_driver()->mock_autofill_download_manager(),
              StartUploadRequest(SignatureIsSameAs(submitted_form), _, _, _, _,
                                 nullptr));
  // Password change vote.
  EXPECT_CALL(*client()->mock_driver()->mock_autofill_download_manager(),
              StartUploadRequest(AllOf(UploadedAutofillTypesAre(expected_types),
                                       HasGenerationVote(false),
                                       SignatureIsSameAs(*observed_form())),
                                 false, expected_available_field_types,
                                 expected_login_signature, true, nullptr));

  EXPECT_CALL(MockFormSaver::Get(&form_manager), Update(_, _, _, _));
  form_manager.Update(*saved_match());
}

// Checks uploading a vote about the usage of the password generation popup.
TEST_F(PasswordFormManagerTest, GeneratedVoteUpload) {
  bool kFalseTrue[] = {false, true};
  SavePromptInteraction kSavePromptInterations[] = {SAVE, NEVER,
                                                    NO_INTERACTION};
  for (bool is_manual_generation : kFalseTrue) {
    for (bool is_change_password_form : kFalseTrue) {
      for (bool has_generated_password : kFalseTrue) {
        for (bool generated_password_changed : kFalseTrue) {
          for (SavePromptInteraction interaction : kSavePromptInterations) {
            GeneratedVoteUploadTest(is_manual_generation,
                                    is_change_password_form,
                                    has_generated_password,
                                    generated_password_changed, interaction);
          }
        }
      }
    }
  }
}

TEST_F(PasswordFormManagerTest, GeneratedPasswordUkm) {
  bool kFalseTrue[] = {false, true};
  SavePromptInteraction kSavePromptInterations[] = {SAVE, NEVER,
                                                    NO_INTERACTION};
  for (bool is_manual_generation : kFalseTrue) {
    for (bool is_change_password_form : kFalseTrue) {
      for (bool has_generated_password : kFalseTrue) {
        for (bool generated_password_changed : kFalseTrue) {
          for (SavePromptInteraction interaction : kSavePromptInterations) {
            GeneratedPasswordUkmTest(is_manual_generation,
                                     is_change_password_form,
                                     has_generated_password,
                                     generated_password_changed, interaction);
          }
        }
      }
    }
  }
}

TEST_F(PasswordFormManagerTest, PresaveGeneratedPasswordAndRemoveIt) {
  PasswordForm credentials = *observed_form();

  // Simulate the user accepted a generated password.
  EXPECT_CALL(MockFormSaver::Get(form_manager()),
              PresaveGeneratedPassword(credentials));
  form_manager()->PresaveGeneratedPassword(credentials);
  EXPECT_TRUE(form_manager()->HasGeneratedPassword());
  EXPECT_FALSE(form_manager()->generated_password_changed());

  // Simulate the user changed the presaved password.
  credentials.password_value = ASCIIToUTF16("changed_password");
  EXPECT_CALL(MockFormSaver::Get(form_manager()),
              PresaveGeneratedPassword(credentials));
  form_manager()->PresaveGeneratedPassword(credentials);
  EXPECT_TRUE(form_manager()->HasGeneratedPassword());
  EXPECT_TRUE(form_manager()->generated_password_changed());

  // Simulate the user removed the presaved password.
  EXPECT_CALL(MockFormSaver::Get(form_manager()), RemovePresavedPassword());
  form_manager()->PasswordNoLongerGenerated();
}

TEST_F(PasswordFormManagerTest, FieldPropertiesMasksUpload) {
  PasswordForm form(*observed_form());
  form.form_data = saved_match()->form_data;

  // Create submitted form.
  PasswordForm submitted_form(form);
  submitted_form.preferred = true;
  submitted_form.username_value = saved_match()->username_value;
  submitted_form.password_value = saved_match()->password_value;

  FakeFormFetcher fetcher;
  fetcher.Fetch();
  PasswordFormManager form_manager(
      password_manager(), client(), client()->driver(), form,
      std::make_unique<NiceMock<MockFormSaver>>(), &fetcher);
  form_manager.Init(nullptr);
  fetcher.SetNonFederated(std::vector<const PasswordForm*>(), 0u);

  DCHECK_EQ(3U, form.form_data.fields.size());
  submitted_form.form_data.fields[1].properties_mask =
      FieldPropertiesFlags::USER_TYPED;
  submitted_form.form_data.fields[2].properties_mask =
      FieldPropertiesFlags::USER_TYPED;

  std::map<base::string16, autofill::FieldPropertiesMask>
      expected_field_properties;
  for (const autofill::FormFieldData& field : submitted_form.form_data.fields)
    if (field.properties_mask)
      expected_field_properties[field.name] = field.properties_mask;

  EXPECT_CALL(*client()->mock_driver()->mock_autofill_download_manager(),
              StartUploadRequest(
                  UploadedFieldPropertiesMasksAre(expected_field_properties),
                  false, _, _, true, nullptr));
  form_manager.ProvisionallySave(submitted_form);
  form_manager.Save();
}

TEST_F(PasswordFormManagerTest, TestSavingAPIFormsWithSamePassword) {
  // Turn |observed_form| and |saved_match| to API created forms.
  observed_form()->username_element.clear();
  observed_form()->type = autofill::PasswordForm::TYPE_API;
  saved_match()->username_element.clear();
  saved_match()->type = autofill::PasswordForm::TYPE_API;

  FakeFormFetcher fetcher;
  fetcher.Fetch();
  PasswordFormManager form_manager(password_manager(), client(),
                                   client()->driver(), *observed_form(),
                                   std::make_unique<MockFormSaver>(), &fetcher);
  form_manager.Init(nullptr);
  fetcher.SetNonFederated({saved_match()}, 0u);

  // User submits new credentials with the same password as in already saved
  // one.
  PasswordForm credentials(*observed_form());
  credentials.username_value =
      saved_match()->username_value + ASCIIToUTF16("1");
  credentials.password_value = saved_match()->password_value;
  credentials.preferred = true;

  form_manager.ProvisionallySave(credentials);

  EXPECT_TRUE(form_manager.IsNewLogin());

  PasswordForm new_credentials;
  EXPECT_CALL(MockFormSaver::Get(&form_manager), Save(_, _))
      .WillOnce(SaveArg<0>(&new_credentials));

  form_manager.Save();

  EXPECT_EQ(saved_match()->username_value + ASCIIToUTF16("1"),
            new_credentials.username_value);
  EXPECT_EQ(saved_match()->password_value, new_credentials.password_value);
  EXPECT_EQ(base::string16(), new_credentials.username_element);
  EXPECT_EQ(autofill::PasswordForm::TYPE_API, new_credentials.type);
}

TEST_F(PasswordFormManagerTest, SkipZeroClickIntact) {
  saved_match()->skip_zero_click = true;
  psl_saved_match()->skip_zero_click = true;
  fake_form_fetcher()->SetNonFederated({saved_match(), psl_saved_match()}, 0u);
  EXPECT_EQ(1u, form_manager()->GetBestMatches().size());

  // User submits a credentials with an old username and a new password.
  PasswordForm credentials(*observed_form());
  credentials.username_value = saved_match()->username_value;
  credentials.password_value = ASCIIToUTF16("new_password");
  credentials.preferred = true;
  form_manager()->ProvisionallySave(credentials);

  // Trigger saving to exercise some special case handling during updating.
  PasswordForm new_credentials;
  std::vector<autofill::PasswordForm> credentials_to_update;
  EXPECT_CALL(MockFormSaver::Get(form_manager()), Update(_, _, _, nullptr))
      .WillOnce(testing::DoAll(SaveArg<0>(&new_credentials),
                               SaveArgPointee<2>(&credentials_to_update)));
  EXPECT_CALL(*client()->mock_driver()->mock_autofill_download_manager(),
              StartUploadRequest(_, false, _, _, true, nullptr));
  form_manager()->Save();

  EXPECT_TRUE(new_credentials.skip_zero_click);
  ASSERT_EQ(1u, credentials_to_update.size());
  EXPECT_TRUE(credentials_to_update[0].skip_zero_click);
}

TEST_F(PasswordFormManagerFillOnAccountSelectTest, ProcessFrame) {
  EXPECT_CALL(*client()->mock_driver(),
              ShowInitialPasswordAccountSuggestions(_));
  fake_form_fetcher()->SetNonFederated({saved_match()}, 0u);
}

// Check that PasswordFormManager records
// PasswordManager_LoginFollowingAutofill as part of processing a credential
// update.
TEST_F(PasswordFormManagerTest, ReportProcessingUpdate) {
  fake_form_fetcher()->SetNonFederated({saved_match()}, 0u);
  PasswordForm pending = *observed_form();
  pending.username_value = saved_match()->username_value;
  pending.password_value = saved_match()->password_value;
  form_manager()->ProvisionallySave(pending);

  EXPECT_FALSE(form_manager()->IsNewLogin());
  EXPECT_CALL(*client()->mock_driver()->mock_autofill_download_manager(),
              StartUploadRequest(_, false, _, _, true, nullptr));

  base::UserActionTester tester;
  EXPECT_EQ(0, tester.GetActionCount("PasswordManager_LoginFollowingAutofill"));
  form_manager()->Update(*saved_match());
  EXPECT_EQ(1, tester.GetActionCount("PasswordManager_LoginFollowingAutofill"));
}

// Sanity check for calling ProcessMatches with empty vector. Should not crash
// or make sanitizers scream.
TEST_F(PasswordFormManagerTest, ProcessMatches_Empty) {
  fake_form_fetcher()->SetNonFederated({}, 0u);
}

// For all combinations of PasswordForm schemes, test that ProcessMatches
// filters out forms with schemes not matching the observed form.
TEST_F(PasswordFormManagerTest, RemoveResultsWithWrongScheme_ObservingHTML) {
  for (int correct = 0; correct <= PasswordForm::SCHEME_LAST; ++correct) {
    for (int wrong = 0; wrong <= PasswordForm::SCHEME_LAST; ++wrong) {
      if (correct == wrong)
        continue;

      const PasswordForm::Scheme kCorrectScheme =
          static_cast<PasswordForm::Scheme>(correct);
      const PasswordForm::Scheme kWrongScheme =
          static_cast<PasswordForm::Scheme>(wrong);
      SCOPED_TRACE(testing::Message() << "Correct scheme = " << kCorrectScheme
                                      << ", wrong scheme = " << kWrongScheme);

      PasswordForm observed = *observed_form();
      observed.scheme = kCorrectScheme;
      FakeFormFetcher fetcher;
      fetcher.Fetch();
      PasswordFormManager form_manager(
          password_manager(), client(),
          (kCorrectScheme == PasswordForm::SCHEME_HTML ? client()->driver()
                                                       : nullptr),
          observed, std::make_unique<NiceMock<MockFormSaver>>(), &fetcher);
      form_manager.Init(nullptr);

      PasswordForm match = *saved_match();
      match.scheme = kCorrectScheme;

      PasswordForm non_match = match;
      non_match.scheme = kWrongScheme;

      // First try putting the correct scheme first in returned matches.
      fetcher.SetNonFederated({&match, &non_match}, 0);

      EXPECT_EQ(1u, form_manager.GetBestMatches().size());
      EXPECT_EQ(kCorrectScheme,
                form_manager.GetBestMatches().begin()->second->scheme);

      // Now try putting the correct scheme last in returned matches.
      fetcher.SetNonFederated({&non_match, &match}, 0);

      EXPECT_EQ(1u, form_manager.GetBestMatches().size());
      EXPECT_EQ(kCorrectScheme,
                form_manager.GetBestMatches().begin()->second->scheme);
    }
  }
}

// Ensure that DoesManage takes into consideration drivers when origins are
// different.
TEST_F(PasswordFormManagerTest, DoesManageDifferentOrigins) {
  for (bool same_drivers : {false, true}) {
    PasswordForm submitted_form(*observed_form());
    observed_form()->origin = GURL("http://accounts.google.com/a/Login");
    submitted_form.origin = GURL("http://accounts.google.com/signin");

    EXPECT_NE(observed_form()->origin, submitted_form.origin);

    NiceMock<MockPasswordManagerDriver> driver;
    EXPECT_EQ(
        same_drivers ? PasswordFormManager::RESULT_COMPLETE_MATCH
                     : PasswordFormManager::RESULT_NO_MATCH,
        form_manager()->DoesManage(
            submitted_form, same_drivers ? client()->driver().get() : &driver));
  }
}

// Ensure that DoesManage returns No match when signon realms are different.
TEST_F(PasswordFormManagerTest, DoesManageDifferentSignonRealmSameDrivers) {
  PasswordForm submitted_form(*observed_form());
  observed_form()->signon_realm = "http://accounts.google.com";
  submitted_form.signon_realm = "http://facebook.com";

  EXPECT_EQ(
      PasswordFormManager::RESULT_NO_MATCH,
      form_manager()->DoesManage(submitted_form, client()->driver().get()));
}

TEST_F(PasswordFormManagerTest, UploadUsernameCorrectionVote) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(kAutofillEnforceMinRequiredFieldsForUpload);
  for (bool is_pending_credential_psl_match : {false, true}) {
      // Observed and saved forms have the same password, but different
      // usernames.
      PasswordForm new_login(*observed_form());
      autofill::FormFieldData field;
      field.name = ASCIIToUTF16("full_name");
      field.form_control_type = "text";
      new_login.form_data.fields.push_back(field);

      field.name = ASCIIToUTF16("observed-username-field");
      field.form_control_type = "text";
      new_login.form_data.fields.push_back(field);

      field.name = ASCIIToUTF16("Passwd");
      field.form_control_type = "password";
      new_login.form_data.fields.push_back(field);

      const PasswordForm* saved_credential =
          is_pending_credential_psl_match ? psl_saved_match() : saved_match();

      new_login.username_value =
          saved_credential->other_possible_usernames[0].first;
      new_login.password_value = saved_credential->password_value;

      PasswordFormManager form_manager(
          password_manager(), client(), client()->driver(), new_login,
          std::make_unique<NiceMock<MockFormSaver>>(), fake_form_fetcher());
      form_manager.Init(nullptr);

      base::HistogramTester histogram_tester;
      fake_form_fetcher()->SetNonFederated({saved_credential}, 0u);
      form_manager.ProvisionallySave(new_login);
      histogram_tester.ExpectUniqueSample(
          "PasswordManager.UsernameCorrectionFound", 1, 1);
      // No match found (because usernames are different).
      EXPECT_TRUE(form_manager.IsNewLogin());

      // Checks the username correction vote is saved.
      PasswordForm expected_username_vote(*saved_credential);
      expected_username_vote.username_element =
          saved_credential->other_possible_usernames[0].second;

      // Checks the upload.
      autofill::ServerFieldTypeSet expected_available_field_types;
      expected_available_field_types.insert(autofill::USERNAME);
      expected_available_field_types.insert(
          autofill::ACCOUNT_CREATION_PASSWORD);

      std::string expected_login_signature =
          FormStructure(form_manager.observed_form().form_data)
              .FormSignatureAsStr();

      std::map<base::string16, autofill::ServerFieldType> expected_types;
      expected_types[expected_username_vote.username_element] =
          autofill::USERNAME;
      expected_types[expected_username_vote.password_element] =
          autofill::ACCOUNT_CREATION_PASSWORD;
      expected_types[ASCIIToUTF16("Email")] = autofill::UNKNOWN_TYPE;

      // Checks the type of the username vote is saved.
      VoteTypeMap expected_vote_types = {
          {expected_username_vote.username_element,
           autofill::AutofillUploadContents::Field::USERNAME_OVERWRITTEN}};

      InSequence s;
      autofill::ServerFieldTypeSet field_types;
      field_types.insert(autofill::PASSWORD);
      EXPECT_CALL(*client()->mock_driver()->mock_autofill_download_manager(),
                  StartUploadRequest(_, false, field_types, std::string(), true,
                                     nullptr));

      EXPECT_CALL(
          *client()->mock_driver()->mock_autofill_download_manager(),
          StartUploadRequest(AllOf(SignatureIsSameAs(expected_username_vote),
                                   UploadedAutofillTypesAre(expected_types),
                                   HasGenerationVote(false),
                                   VoteTypesAre(expected_vote_types)),
                             false, expected_available_field_types,
                             expected_login_signature, true, nullptr));
      form_manager.Save();
  }
}

TEST_F(PasswordFormManagerTest, NoUsernameCorrectionVote) {
  fake_form_fetcher()->SetNonFederated({saved_match()}, 0u);
  PasswordForm new_login = *observed_form();
  // The username is from |saved_match_.other_possible_usernames|, but the
  // password is different. So, no username correction found.
  new_login.username_value = ASCIIToUTF16("test2@gmail.com");
  new_login.password_value = ASCIIToUTF16("newpass");

  base::HistogramTester histogram_tester;
  form_manager()->ProvisionallySave(new_login);
  histogram_tester.ExpectUniqueSample("PasswordManager.UsernameCorrectionFound",
                                      0, 1);

  // Don't expect any vote uploads.
  EXPECT_CALL(*client()->mock_driver()->mock_autofill_download_manager(),
              StartUploadRequest(_, _, _, _, _, _))
      .Times(0);

  form_manager()->Save();
}

TEST_F(PasswordFormManagerTest,
       UsernameCorrectionVote_PasswordReusedWithoutUsername) {
  for (bool saved_username_is_empty : {false, true}) {
    SCOPED_TRACE(testing::Message()
                 << "saved_username_is_empty=" << saved_username_is_empty);

    if (saved_username_is_empty) {
      saved_match()->username_value.clear();
      // |username_element| must be empty as well, but it may be non-empty in
      // credentials saved in past versions. Let's test there will be not vote
      // for this element even if |username_element| has a non-empty value.
    }

    saved_match()->other_possible_usernames.push_back(
        ValueElementPair(base::string16(), ASCIIToUTF16("empty_field")));
    fake_form_fetcher()->SetNonFederated({saved_match()}, 0u);

    // A user enters the password in the form. The username is absent or not
    // captured, but the user doesn't enter the username in a prompt.
    PasswordForm credential(*saved_match());
    credential.username_value.clear();
    credential.preferred = true;
    form_manager()->ProvisionallySave(credential);
    EXPECT_FALSE(form_manager()->IsNewLogin());

    // Create the expected credential to be saved.
    PasswordForm expected_pending(*saved_match());
    expected_pending.times_used = 1;
    // As a credential is reused, |other_possible_usernames| will be cleared.
    // In fact, the username wasn't reused, only password, but for the sake of
    // simplicity |other_possible_usernames| is cleared even in this case.
    expected_pending.other_possible_usernames.clear();

    EXPECT_CALL(MockFormSaver::Get(form_manager()),
                Update(expected_pending,
                       ElementsAre(Pair(saved_match()->username_value,
                                        Pointee(*saved_match()))),
                       Pointee(IsEmpty()), nullptr));
    EXPECT_CALL(*client()->mock_driver()->mock_autofill_download_manager(),
                StartUploadRequest(_, _, _, _, _, nullptr));
    EXPECT_CALL(*client()->mock_driver()->mock_autofill_download_manager(),
                StartUploadRequest(_, _, Not(Contains(autofill::USERNAME)), _,
                                   _, nullptr));
    form_manager()->Save();
  }
}

// Test that ResetStoredMatches removes references to previously fetched store
// results.
TEST_F(PasswordFormManagerTest, ResetStoredMatches) {
  PasswordForm best_match1 = *observed_form();
  best_match1.username_value = ASCIIToUTF16("user1");
  best_match1.password_value = ASCIIToUTF16("pass");
  best_match1.preferred = true;

  PasswordForm best_match2 = best_match1;
  best_match2.username_value = ASCIIToUTF16("user2");
  best_match2.preferred = false;

  PasswordForm non_best_match = best_match1;
  non_best_match.action = GURL();

  PasswordForm blacklisted = *observed_form();
  blacklisted.blacklisted_by_user = true;
  blacklisted.username_value.clear();

  fake_form_fetcher()->SetNonFederated(
      {&best_match1, &best_match2, &non_best_match, &blacklisted}, 0u);

  EXPECT_EQ(2u, form_manager()->GetBestMatches().size());
  EXPECT_TRUE(form_manager()->GetPreferredMatch());
  EXPECT_EQ(1u, form_manager()->GetBlacklistedMatches().size());

  // Trigger Update to verify that there is a non-best match.
  PasswordForm updated(best_match1);
  updated.password_value = ASCIIToUTF16("updated password");
  form_manager()->ProvisionallySave(updated);
  std::vector<PasswordForm> credentials_to_update;
  EXPECT_CALL(MockFormSaver::Get(form_manager()), Update(_, _, _, nullptr))
      .WillOnce(SaveArgPointee<2>(&credentials_to_update));
  form_manager()->Save();

  PasswordForm updated_non_best = non_best_match;
  updated_non_best.password_value = updated.password_value;
  EXPECT_THAT(credentials_to_update, UnorderedElementsAre(updated_non_best));

  form_manager()->ResetStoredMatches();

  EXPECT_THAT(form_manager()->GetBestMatches(), IsEmpty());
  EXPECT_FALSE(form_manager()->GetPreferredMatch());
  EXPECT_THAT(form_manager()->GetBlacklistedMatches(), IsEmpty());

  // Simulate updating a saved credential again, but this time without non-best
  // matches. Verify that the old non-best matches are no longer present.
  fake_form_fetcher()->Fetch();
  fake_form_fetcher()->SetNonFederated({&best_match1}, 0u);

  form_manager()->ProvisionallySave(updated);
  credentials_to_update.clear();
  EXPECT_CALL(MockFormSaver::Get(form_manager()), Update(_, _, _, nullptr))
      .WillOnce(SaveArgPointee<2>(&credentials_to_update));
  form_manager()->Save();

  EXPECT_THAT(credentials_to_update, IsEmpty());
}

// Check that on changing FormFetcher, the PasswordFormManager removes itself
// from consuming the old one.
TEST_F(PasswordFormManagerTest, DropFetcherOnDestruction) {
  MockFormFetcher fetcher;
  FormFetcher::Consumer* added_consumer = nullptr;
  EXPECT_CALL(fetcher, AddConsumer(_)).WillOnce(SaveArg<0>(&added_consumer));
  auto form_manager = std::make_unique<PasswordFormManager>(
      password_manager(), client(), client()->driver(), *observed_form(),
      std::make_unique<MockFormSaver>(), &fetcher);
  form_manager->Init(nullptr);
  EXPECT_EQ(form_manager.get(), added_consumer);

  EXPECT_CALL(fetcher, RemoveConsumer(form_manager.get()));
  form_manager.reset();
}

// Check that if asked to take ownership of the same FormFetcher which it had
// consumed before, the PasswordFormManager does not add itself as a consumer
// again.
TEST_F(PasswordFormManagerTest, GrabFetcher_Same) {
  auto fetcher = std::make_unique<MockFormFetcher>();
  fetcher->Fetch();
  PasswordFormManager form_manager(
      password_manager(), client(), client()->driver(), *observed_form(),
      std::make_unique<MockFormSaver>(), fetcher.get());
  form_manager.Init(nullptr);

  EXPECT_CALL(*fetcher, AddConsumer(_)).Times(0);
  EXPECT_CALL(*fetcher, RemoveConsumer(_)).Times(0);
  form_manager.GrabFetcher(std::move(fetcher));
  // There will be a RemoveConsumer call as soon as form_manager goes out of
  // scope, but the test needs to ensure that there is none as a result of
  // GrabFetcher.
  Mock::VerifyAndClearExpectations(form_manager.GetFormFetcher());
}

// Check that if asked to take ownership of a different FormFetcher than which
// it had consumed before, the PasswordFormManager adds itself as a consumer
// and replaces the references to the old results.
TEST_F(PasswordFormManagerTest, GrabFetcher_Different) {
  PasswordForm old_match = *observed_form();
  old_match.username_value = ASCIIToUTF16("user1");
  old_match.password_value = ASCIIToUTF16("pass");
  fake_form_fetcher()->SetNonFederated({&old_match}, 0u);
  EXPECT_EQ(1u, form_manager()->GetBestMatches().size());
  EXPECT_EQ(&old_match, form_manager()->GetBestMatches().begin()->second);

  // |form_manager()| uses |fake_form_fetcher()|, which is an instance
  // different from |fetcher| below.
  auto fetcher = std::make_unique<MockFormFetcher>();
  fetcher->Fetch();
  fetcher->SetNonFederated(std::vector<const PasswordForm*>(), 0u);
  EXPECT_CALL(*fetcher, AddConsumer(form_manager()));
  form_manager()->GrabFetcher(std::move(fetcher));

  EXPECT_EQ(0u, form_manager()->GetBestMatches().size());
}

// Check that on changing FormFetcher, the PasswordFormManager removes itself
// from consuming the old one.
TEST_F(PasswordFormManagerTest, GrabFetcher_Remove) {
  MockFormFetcher old_fetcher;
  FormFetcher::Consumer* added_consumer = nullptr;
  EXPECT_CALL(old_fetcher, AddConsumer(_))
      .WillOnce(SaveArg<0>(&added_consumer));
  PasswordFormManager form_manager(
      password_manager(), client(), client()->driver(), *observed_form(),
      std::make_unique<MockFormSaver>(), &old_fetcher);
  form_manager.Init(nullptr);
  EXPECT_EQ(&form_manager, added_consumer);

  auto new_fetcher = std::make_unique<MockFormFetcher>();
  EXPECT_CALL(*new_fetcher, AddConsumer(&form_manager));
  EXPECT_CALL(old_fetcher, RemoveConsumer(&form_manager));
  form_manager.GrabFetcher(std::move(new_fetcher));
}

TEST_F(PasswordFormManagerTest,
       SuppressedHTTPSFormsHistogram_NotRecordedIfStoreWasTooSlow) {
  base::HistogramTester histogram_tester;

  fake_form_fetcher()->set_did_complete_querying_suppressed_forms(false);
  fake_form_fetcher()->Fetch();
  std::unique_ptr<PasswordFormManager> form_manager =
      std::make_unique<PasswordFormManager>(
          password_manager(), client(), client()->driver(), *observed_form(),
          std::make_unique<NiceMock<MockFormSaver>>(), fake_form_fetcher());
  form_manager->Init(nullptr);
  fake_form_fetcher()->SetNonFederated(std::vector<const PasswordForm*>(), 0u);
  form_manager.reset();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.QueryingSuppressedAccountsFinished", false, 1);
  const auto sample_counts = histogram_tester.GetTotalCountsForPrefix(
      "PasswordManager.SuppressedAccount.");
  EXPECT_THAT(sample_counts, IsEmpty());
}

TEST_F(PasswordFormManagerTest, SuppressedFormsHistograms) {
  static constexpr const struct {
    SuppressedFormType type;
    const char* expected_histogram_suffix;
    void (FakeFormFetcher::*suppressed_forms_setter_func)(
        const std::vector<const autofill::PasswordForm*>&);
  } kSuppressedFormTypeParams[] = {
      {SuppressedFormType::HTTPS, "HTTPSNotHTTP",
       &FakeFormFetcher::set_suppressed_https_forms},
      {SuppressedFormType::PSL_MATCH, "PSLMatching",
       &FakeFormFetcher::set_suppressed_psl_matching_forms},
      {SuppressedFormType::SAME_ORGANIZATION_NAME, "SameOrganizationName",
       &FakeFormFetcher::set_suppressed_same_organization_name_forms}};

  struct SuppressedFormData {
    const char* username_value;
    const char* password_value;
    PasswordForm::Type manual_or_generated;
  };

  static constexpr const char kUsernameAlpha[] = "user-alpha@gmail.com";
  static constexpr const char kPasswordAlpha[] = "password-alpha";
  static constexpr const char kUsernameBeta[] = "user-beta@gmail.com";
  static constexpr const char kPasswordBeta[] = "password-beta";

  static constexpr const SuppressedFormData kSuppressedGeneratedForm = {
      kUsernameAlpha, kPasswordAlpha, PasswordForm::TYPE_GENERATED};
  static constexpr const SuppressedFormData kOtherSuppressedGeneratedForm = {
      kUsernameBeta, kPasswordBeta, PasswordForm::TYPE_GENERATED};
  static constexpr const SuppressedFormData kSuppressedManualForm = {
      kUsernameAlpha, kPasswordBeta, PasswordForm::TYPE_MANUAL};

  const std::vector<const SuppressedFormData*> kSuppressedFormsNone;
  const std::vector<const SuppressedFormData*> kSuppressedFormsOneGenerated = {
      &kSuppressedGeneratedForm};
  const std::vector<const SuppressedFormData*> kSuppressedFormsTwoGenerated = {
      &kSuppressedGeneratedForm, &kOtherSuppressedGeneratedForm};
  const std::vector<const SuppressedFormData*> kSuppressedFormsOneManual = {
      &kSuppressedManualForm};
  const std::vector<const SuppressedFormData*> kSuppressedFormsTwoMixed = {
      &kSuppressedGeneratedForm, &kSuppressedManualForm};

  const struct {
    std::vector<const SuppressedFormData*> simulated_suppressed_forms;
    SimulatedManagerAction simulate_manager_action;
    SimulatedSubmitResult simulate_submit_result;
    const char* filled_username;
    const char* filled_password;
    int expected_histogram_sample_generated;
    int expected_histogram_sample_manual;
    const char* submitted_password;  // nullptr if same as |filled_password|.
  } kTestCases[] = {
      // See PasswordManagerSuppressedAccountCrossActionsTaken in enums.xml.
      //
      // Legend: (SuppressAccountType, SubmitResult, SimulatedManagerAction,
      // UserAction)
      // 24 = (None, Passed, None, OverrideUsernameAndPassword)
      {kSuppressedFormsNone, SimulatedManagerAction::NONE,
       SimulatedSubmitResult::PASSED, kUsernameAlpha, kPasswordAlpha, 24, 24},
      // 5 = (None, NotSubmitted, Autofilled, None)
      {kSuppressedFormsNone, SimulatedManagerAction::AUTOFILLED,
       SimulatedSubmitResult::NONE, kUsernameAlpha, kPasswordAlpha, 5, 5},
      // 15 = (None, Failed, Autofilled, None)
      {kSuppressedFormsNone, SimulatedManagerAction::AUTOFILLED,
       SimulatedSubmitResult::FAILED, kUsernameAlpha, kPasswordAlpha, 15, 15},

      // 35 = (Exists, NotSubmitted, Autofilled, None)
      {kSuppressedFormsOneGenerated, SimulatedManagerAction::AUTOFILLED,
       SimulatedSubmitResult::NONE, kUsernameAlpha, kPasswordAlpha, 35, 5},
      // 145 = (ExistsSameUsernameAndPassword, Passed, Autofilled, None)
      // 25 = (None, Passed, Autofilled, None)
      {kSuppressedFormsOneGenerated, SimulatedManagerAction::AUTOFILLED,
       SimulatedSubmitResult::PASSED, kUsernameAlpha, kPasswordAlpha, 145, 25},
      // 104 = (ExistsSameUsername, Failed, None, OverrideUsernameAndPassword)
      // 14 = (None, Failed, None, OverrideUsernameAndPassword)
      {kSuppressedFormsOneGenerated, SimulatedManagerAction::NONE,
       SimulatedSubmitResult::FAILED, kUsernameAlpha, kPasswordBeta, 104, 14},
      // 84 = (ExistsDifferentUsername, Passed, None,
      //       OverrideUsernameAndPassword)
      {kSuppressedFormsOneGenerated, SimulatedManagerAction::NONE,
       SimulatedSubmitResult::PASSED, kUsernameBeta, kPasswordAlpha, 84, 24},

      // 144 = (ExistsSameUsernameAndPassword, Passed, None,
      //        OverrideUsernameAndPassword)
      {kSuppressedFormsOneManual, SimulatedManagerAction::NONE,
       SimulatedSubmitResult::PASSED, kUsernameAlpha, kPasswordBeta, 24, 144},
      {kSuppressedFormsTwoMixed, SimulatedManagerAction::NONE,
       SimulatedSubmitResult::PASSED, kUsernameBeta, kPasswordAlpha, 84, 84},

      // 115 = (ExistsSameUsername, Passed, Autofilled, None)
      {kSuppressedFormsTwoGenerated, SimulatedManagerAction::AUTOFILLED,
       SimulatedSubmitResult::PASSED, kUsernameAlpha, kPasswordAlpha, 145, 25},
      {kSuppressedFormsTwoGenerated, SimulatedManagerAction::AUTOFILLED,
       SimulatedSubmitResult::PASSED, kUsernameAlpha, kPasswordBeta, 115, 25},
      {kSuppressedFormsTwoGenerated, SimulatedManagerAction::AUTOFILLED,
       SimulatedSubmitResult::PASSED, kUsernameBeta, kPasswordAlpha, 115, 25},
      {kSuppressedFormsTwoGenerated, SimulatedManagerAction::AUTOFILLED,
       SimulatedSubmitResult::PASSED, kUsernameBeta, kPasswordBeta, 145, 25},

      // 86 = (ExistsDifferentUsername, Passed, Autofilled, Choose)
      // 26 = (None, Passed, Autofilled, Choose)
      {kSuppressedFormsOneGenerated, SimulatedManagerAction::OFFERED,
       SimulatedSubmitResult::PASSED, kUsernameBeta, kPasswordAlpha, 86, 26},
      // 72 = (ExistsDifferentUsername, Failed, None, ChoosePSL)
      // 12 = (None, Failed, None, ChoosePSL)
      {kSuppressedFormsOneGenerated, SimulatedManagerAction::OFFERED_PSL,
       SimulatedSubmitResult::FAILED, kUsernameBeta, kPasswordAlpha, 72, 12},
      // 148 = (ExistsSameUsernameAndPassword, Passed, Autofilled,
      //        OverridePassword)
      // 28 = (None, Passed, Autofilled, OverridePassword)
      {kSuppressedFormsTwoGenerated, SimulatedManagerAction::AUTOFILLED,
       SimulatedSubmitResult::PASSED, kUsernameBeta, kPasswordAlpha, 148, 28,
       kPasswordBeta},
  };

  for (const auto& suppression_params : kSuppressedFormTypeParams) {
    for (const auto& test_case : kTestCases) {
      SCOPED_TRACE(suppression_params.expected_histogram_suffix);
      SCOPED_TRACE(test_case.expected_histogram_sample_manual);
      SCOPED_TRACE(test_case.expected_histogram_sample_generated);

      base::HistogramTester histogram_tester;
      ukm::TestAutoSetUkmRecorder test_ukm_recorder;

      std::vector<PasswordForm> suppressed_forms;
      for (const auto* form_data : test_case.simulated_suppressed_forms) {
        suppressed_forms.push_back(CreateSuppressedForm(
            suppression_params.type, form_data->username_value,
            form_data->password_value, form_data->manual_or_generated));
      }

      std::vector<const PasswordForm*> suppressed_forms_ptrs;
      for (const auto& form : suppressed_forms)
        suppressed_forms_ptrs.push_back(&form);

      FakeFormFetcher fetcher;
      fetcher.set_did_complete_querying_suppressed_forms(true);

      (&fetcher->*suppression_params.suppressed_forms_setter_func)(
          suppressed_forms_ptrs);

      SimulateActionsOnHTTPObservedForm(
          &fetcher, test_case.simulate_manager_action,
          test_case.simulate_submit_result, test_case.filled_username,
          test_case.filled_password, test_case.submitted_password);

      histogram_tester.ExpectUniqueSample(
          "PasswordManager.QueryingSuppressedAccountsFinished", true, 1);

      EXPECT_THAT(
          histogram_tester.GetAllSamples(
              "PasswordManager.SuppressedAccount.Generated." +
              std::string(suppression_params.expected_histogram_suffix)),
          ::testing::ElementsAre(
              base::Bucket(test_case.expected_histogram_sample_generated, 1)));
      EXPECT_THAT(
          histogram_tester.GetAllSamples(
              "PasswordManager.SuppressedAccount.Manual." +
              std::string(suppression_params.expected_histogram_suffix)),
          ::testing::ElementsAre(
              base::Bucket(test_case.expected_histogram_sample_manual, 1)));

      auto entries = test_ukm_recorder.GetEntriesByName(
          ukm::builders::PasswordForm::kEntryName);
      EXPECT_EQ(1u, entries.size());
      for (const auto* const entry : entries) {
        test_ukm_recorder.ExpectEntryMetric(
            entry,
            "SuppressedAccount.Generated." +
                std::string(suppression_params.expected_histogram_suffix),
            test_case.expected_histogram_sample_generated /
                PasswordFormMetricsRecorder::kMaxNumActionsTakenNew);
        test_ukm_recorder.ExpectEntryMetric(
            entry,
            "SuppressedAccount.Manual." +
                std::string(suppression_params.expected_histogram_suffix),
            test_case.expected_histogram_sample_manual /
                PasswordFormMetricsRecorder::kMaxNumActionsTakenNew);
      }
    }
  }
}

// If the frame containing the login form is served over HTTPS, no histograms on
// supressed HTTPS forms should be recorded. Everything else should still be.
TEST_F(PasswordFormManagerTest, SuppressedHTTPSFormsHistogram_NotRecordedFor) {
  base::HistogramTester histogram_tester;

  PasswordForm https_observed_form = *observed_form();
  https_observed_form.origin = GURL("https://accounts.google.com/a/LoginAuth");
  https_observed_form.signon_realm = "https://accounts.google.com/";

  // Only the scheme of the frame containing the login form maters, not the
  // scheme of the main frame.
  ASSERT_FALSE(client()->IsMainFrameSecure());

  // Even if there were any suppressed HTTPS forms, they should be are ignored
  // (but there should be none in production in this case).
  FakeFormFetcher fetcher;
  fetcher.set_suppressed_https_forms({saved_match()});
  fetcher.set_did_complete_querying_suppressed_forms(true);
  fetcher.Fetch();

  std::unique_ptr<PasswordFormManager> form_manager =
      std::make_unique<PasswordFormManager>(
          password_manager(), client(), client()->driver(), https_observed_form,
          std::make_unique<NiceMock<MockFormSaver>>(), &fetcher);
  form_manager->Init(nullptr);
  fetcher.SetNonFederated(std::vector<const PasswordForm*>(), 0u);
  form_manager.reset();

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.QueryingSuppressedAccountsFinished", true, 1);
  histogram_tester.ExpectTotalCount(
      "PasswordManager.SuppressedAccount.Generated.HTTPSNotHTTP", 0);
  histogram_tester.ExpectTotalCount(
      "PasswordManager.SuppressedAccount.Manual.HTTPSNotHTTP", 0);
  histogram_tester.ExpectTotalCount(
      "PasswordManager.SuppressedAccount.Generated.PSLMatching", 1);
  histogram_tester.ExpectTotalCount(
      "PasswordManager.SuppressedAccount.Manual.PSLMatching", 1);
  histogram_tester.ExpectTotalCount(
      "PasswordManager.SuppressedAccount.Generated.SameOrganizationName", 1);
  histogram_tester.ExpectTotalCount(
      "PasswordManager.SuppressedAccount.Manual.SameOrganizationName", 1);
}

// Check that a cloned PasswordFormManager reacts correctly to Save.
TEST_F(PasswordFormManagerTest, Clone_OnSave) {
  FakeFormFetcher fetcher;
  auto form_manager = std::make_unique<PasswordFormManager>(
      password_manager(), client(), client()->driver(), *observed_form(),
      std::make_unique<MockFormSaver>(), &fetcher);
  form_manager->Init(nullptr);
  fetcher.SetNonFederated(std::vector<const PasswordForm*>(), 0u);

  PasswordForm saved_login = *observed_form();
  saved_login.username_value = ASCIIToUTF16("newuser");
  saved_login.password_value = ASCIIToUTF16("newpass");
  form_manager->ProvisionallySave(saved_login);

  const PasswordForm pending = form_manager->GetPendingCredentials();

  std::unique_ptr<PasswordFormManager> clone = form_manager->Clone();

  PasswordForm passed;
  EXPECT_CALL(MockFormSaver::Get(clone.get()), Save(_, IsEmpty()))
      .WillOnce(SaveArg<0>(&passed));
  clone->Save();
  // The date is expected to be different. Reset it so that we can easily
  // compare the rest with operator==.
  passed.date_created = pending.date_created;
  EXPECT_EQ(pending, passed);
}

// Check that a cloned PasswordFormManager reacts correctly to OnNeverClicked.
TEST_F(PasswordFormManagerTest, Clone_OnNeverClicked) {
  FakeFormFetcher fetcher;
  auto form_manager = std::make_unique<PasswordFormManager>(
      password_manager(), client(), client()->driver(), *observed_form(),
      std::make_unique<MockFormSaver>(), &fetcher);
  form_manager->Init(nullptr);
  fetcher.SetNonFederated(std::vector<const PasswordForm*>(), 0u);

  PasswordForm saved_login = *observed_form();
  saved_login.username_value = ASCIIToUTF16("newuser");
  saved_login.password_value = ASCIIToUTF16("newpass");
  form_manager->ProvisionallySave(saved_login);

  std::unique_ptr<PasswordFormManager> clone = form_manager->Clone();

  EXPECT_CALL(MockFormSaver::Get(clone.get()),
              PermanentlyBlacklist(Pointee(*observed_form())));
  clone->OnNeverClicked();
}

// Check that a cloned PasswordFormManager works even after the original is
// gone.
TEST_F(PasswordFormManagerTest, Clone_SurvivesOriginal) {
  FakeFormFetcher fetcher;
  auto form_manager = std::make_unique<PasswordFormManager>(
      password_manager(), client(), client()->driver(), *observed_form(),
      std::make_unique<MockFormSaver>(), &fetcher);
  fetcher.SetNonFederated(std::vector<const PasswordForm*>(), 0u);
  form_manager->Init(nullptr);

  PasswordForm saved_login = *observed_form();
  saved_login.username_value = ASCIIToUTF16("newuser");
  saved_login.password_value = ASCIIToUTF16("newpass");
  form_manager->ProvisionallySave(saved_login);

  const PasswordForm pending = form_manager->GetPendingCredentials();

  std::unique_ptr<PasswordFormManager> clone = form_manager->Clone();
  form_manager.reset();

  PasswordForm passed;
  EXPECT_CALL(MockFormSaver::Get(clone.get()), Save(_, IsEmpty()))
      .WillOnce(SaveArg<0>(&passed));
  clone->Save();
  // The date is expected to be different. Reset it so that we can easily
  // compare the rest with operator==.
  passed.date_created = pending.date_created;
  EXPECT_EQ(pending, passed);
}

// Verifies that URL keyed metrics are recorded for the filling of passwords
// into forms and HTTP Basic auth.
TEST_F(PasswordFormManagerTest, TestUkmForFilling) {
  PasswordForm scheme_basic_form = *observed_form();
  scheme_basic_form.scheme = PasswordForm::SCHEME_BASIC;

  const struct {
    // Whether the login is doing HTTP Basic Auth instead of form based login.
    bool is_http_basic_auth;
    // The matching stored credentials, or nullptr if none.
    PasswordForm* fetched_form;
    PasswordFormMetricsRecorder::ManagerAutofillEvent expected_event;
  } kTestCases[] = {
      {false, saved_match(),
       PasswordFormMetricsRecorder::kManagerFillEventAutofilled},
      {false, psl_saved_match(),
       PasswordFormMetricsRecorder::kManagerFillEventBlockedOnInteraction},
      {false, nullptr,
       PasswordFormMetricsRecorder::kManagerFillEventNoCredential},
      {true, &scheme_basic_form,
       PasswordFormMetricsRecorder::kManagerFillEventAutofilled},
      {true, nullptr,
       PasswordFormMetricsRecorder::kManagerFillEventNoCredential},
  };

  for (const auto& test : kTestCases) {
    const PasswordForm& form_to_fill =
        test.is_http_basic_auth ? scheme_basic_form : *observed_form();

    std::vector<const autofill::PasswordForm*> fetched_forms;
    if (test.fetched_form)
      fetched_forms.push_back(test.fetched_form);

    ukm::TestAutoSetUkmRecorder test_ukm_recorder;
    {
      auto metrics_recorder = base::MakeRefCounted<PasswordFormMetricsRecorder>(
          form_to_fill.origin.SchemeIsCryptographic(),
          client()->GetUkmSourceId());
      FakeFormFetcher fetcher;
      PasswordFormManager form_manager(
          password_manager(), client(),
          test.is_http_basic_auth ? nullptr : client()->driver(), form_to_fill,
          std::make_unique<NiceMock<MockFormSaver>>(), &fetcher);
      form_manager.Init(metrics_recorder);
      fetcher.SetNonFederated(fetched_forms, 0u);
    }

    auto entries = test_ukm_recorder.GetEntriesByName(
        ukm::builders::PasswordForm::kEntryName);
    EXPECT_EQ(1u, entries.size());
    for (const auto* const entry : entries) {
      EXPECT_EQ(client()->GetUkmSourceId(), entry->source_id);
      test_ukm_recorder.ExpectEntryMetric(
          entry, ukm::builders::PasswordForm::kManagerFill_ActionName,
          test.expected_event);
    }
  }
}

// Verifies that the form signature of forms is recorded in UKMs.
TEST_F(PasswordFormManagerTest, TestUkmContextMetrics) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  test_ukm_recorder.UpdateSourceURL(client()->GetUkmSourceId(),
                                    client()->GetMainFrameURL());

  // Register two forms on one page.
  PasswordForm second_observed_form = *observed_form();
  second_observed_form.form_data.action = GURL("https://somewhere-else.com");
  for (PasswordForm* form : {observed_form(), &second_observed_form}) {
    auto metrics_recorder = base::MakeRefCounted<PasswordFormMetricsRecorder>(
        form->origin.SchemeIsCryptographic(), client()->GetUkmSourceId());
    FakeFormFetcher fetcher;
    PasswordFormManager form_manager(
        password_manager(), client(), client()->driver(), *form,
        std::make_unique<NiceMock<MockFormSaver>>(), &fetcher);
    form_manager.Init(metrics_recorder);
  }

  // Verify that a form signatures have been recorded in UKM.
  int64_t form_signature_1 = PasswordFormMetricsRecorder::HashFormSignature(
      CalculateFormSignature(observed_form()->form_data));
  int64_t form_signature_2 = PasswordFormMetricsRecorder::HashFormSignature(
      CalculateFormSignature(second_observed_form.form_data));

  EXPECT_GE(form_signature_1, 0);
  EXPECT_GE(form_signature_2, 0);

  auto entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::PasswordForm::kEntryName);
  ASSERT_EQ(2u, entries.size());

  const int64_t* metric1 = test_ukm_recorder.GetEntryMetric(
      entries[0], ukm::builders::PasswordForm::kContext_FormSignatureName);
  const int64_t* metric2 = test_ukm_recorder.GetEntryMetric(
      entries[1], ukm::builders::PasswordForm::kContext_FormSignatureName);

  ASSERT_TRUE(metric1);
  ASSERT_TRUE(metric2);

  EXPECT_THAT(
      std::vector<int64_t>({*metric1, *metric2}),
      ::testing::UnorderedElementsAre(form_signature_1, form_signature_2));
}

TEST_F(PasswordFormManagerTest,
       TestNotSendNotBlacklistedMessage_BlacklistedCredentials) {
  // Signing up on a previously visited site. Credentials are found in the
  // password store, but they are blacklisted. AllowPasswordGenerationForForm
  // is not called.
  EXPECT_CALL(*(client()->mock_driver()), AllowPasswordGenerationForForm(_))
      .Times(0);
  PasswordForm simulated_result = CreateSavedMatch(true);
  fake_form_fetcher()->SetNonFederated({&simulated_result}, 0u);
}

TEST_F(PasswordFormManagerTest, FirstLoginVote) {
  PasswordForm old_without_username = *saved_match();
  old_without_username.username_value.clear();
  old_without_username.username_element.clear();
  old_without_username.times_used = 2;
  struct {
    std::vector<const autofill::PasswordForm*> stored_creds;
    std::string description;
  } test_cases[] = {
      {{saved_match()}, "Credential reused"},
      {{psl_saved_match()}, "PSL credential reused"},
      {{&old_without_username},
       "Submitted credential adds a username to a stored credential without "
       "one"},
  };

  for (auto test_case : test_cases) {
    SCOPED_TRACE(testing::Message()
                 << "Stored credentials: " << test_case.description);

    fake_form_fetcher()->SetNonFederated(test_case.stored_creds, 0u);

    PasswordForm submitted_form =
        CreateMinimalCrowdsourcableForm(*observed_form());
    submitted_form.username_value = saved_match()->username_value;
    submitted_form.password_value = saved_match()->password_value;
    submitted_form.form_data.fields[0].value = submitted_form.username_value;
    submitted_form.form_data.fields[1].value = submitted_form.password_value;
    submitted_form.preferred = true;
    EXPECT_TRUE(FormStructure(submitted_form.form_data).ShouldBeUploaded());

    form_manager()->ProvisionallySave(submitted_form);

    // The username and password fields contain stored values. This should be
    // signaled in the vote.
    std::map<base::string16, autofill::FieldPropertiesMask>
        expected_field_properties = {{submitted_form.username_element,
                                      FieldPropertiesFlags::KNOWN_VALUE},
                                     {submitted_form.password_element,
                                      FieldPropertiesFlags::KNOWN_VALUE}};

    // All votes should be FIRST_USE.
    std::map<base::string16, autofill::AutofillUploadContents::Field::VoteType>
        expected_vote_types = {
            {submitted_form.username_element,
             autofill::AutofillUploadContents::Field::FIRST_USE},
            {submitted_form.password_element,
             autofill::AutofillUploadContents::Field::FIRST_USE}};

    // Unrelated vote
    EXPECT_CALL(
        *client()->mock_driver()->mock_autofill_download_manager(),
        StartUploadRequest(SignatureIsSameAs(*test_case.stored_creds.front()),
                           _, _, _, _, _))
        .Times(AtMost(1));
    // First login vote
    EXPECT_CALL(
        *client()->mock_driver()->mock_autofill_download_manager(),
        StartUploadRequest(
            AllOf(SignatureIsSameAs(submitted_form),
                  UploadedAutofillTypesAre(FieldTypeMap(
                      {{submitted_form.username_element, autofill::USERNAME},
                       {submitted_form.password_element, autofill::PASSWORD},
                       {ASCIIToUTF16("petname"), autofill::UNKNOWN_TYPE}})),
                  UploadedFieldPropertiesMasksAre(expected_field_properties),
                  VoteTypesAre(expected_vote_types)),
            _,
            autofill::ServerFieldTypeSet(
                {autofill::USERNAME, autofill::PASSWORD}),
            _, true, nullptr));

    form_manager()->Save();

    Mock::VerifyAndClearExpectations(
        client()->mock_driver()->mock_autofill_download_manager());
  }
}

// Tests scenarios where no vote should be uploaded.
TEST_F(PasswordFormManagerTest, FirstLoginVote_NoVote) {
  PasswordForm old_credential(*saved_match());
  old_credential.times_used = 1;

  // A new username means a new credential. We will vote on it the next time it
  // is used, not now.
  PasswordForm different_username(*saved_match());
  different_username.username_value = ASCIIToUTF16("DifferentUsername");

  struct {
    std::vector<const autofill::PasswordForm*> stored_creds;
    std::string description;
  } test_cases[] = {
      {{}, "No credentials stored"},
      {{&old_credential}, "Not first use"},
      {{&different_username}, "Different username"},
  };

  for (auto test_case : test_cases) {
    SCOPED_TRACE(testing::Message()
                 << "Stored credentials: " << test_case.description);

    fake_form_fetcher()->SetNonFederated(test_case.stored_creds, 0u);

    // User submits credentials for the observed form.
    PasswordForm submitted_form =
        CreateMinimalCrowdsourcableForm(*observed_form());
    submitted_form.username_value = saved_match()->username_value;
    submitted_form.password_value = saved_match()->password_value;
    submitted_form.form_data.fields[0].value = submitted_form.username_value;
    submitted_form.form_data.fields[1].value = submitted_form.password_value;
    submitted_form.preferred = true;
    EXPECT_TRUE(FormStructure(submitted_form.form_data).ShouldBeUploaded());

    form_manager()->ProvisionallySave(submitted_form);

    EXPECT_CALL(*client()->mock_driver()->mock_autofill_download_manager(),
                StartUploadRequest(_, _, _, _, _, _))
        .Times(0);

    form_manager()->Save();

    Mock::VerifyAndClearExpectations(
        client()->mock_driver()->mock_autofill_download_manager());
  }
}

// If we update an existing credential with a new password, only the username is
// a known value.
TEST_F(PasswordFormManagerTest,
       FirstLoginVote_UpdatePasswordVotesOnlyForUsername) {
  PasswordForm different_password(*saved_match());
  different_password.password_value = ASCIIToUTF16("DifferentPassword");
  fake_form_fetcher()->SetNonFederated({&different_password}, 0u);

  PasswordForm submitted_form =
      CreateMinimalCrowdsourcableForm(*observed_form());
  submitted_form.username_value = saved_match()->username_value;
  submitted_form.password_value = saved_match()->password_value;
  submitted_form.form_data.fields[0].value = submitted_form.username_value;
  submitted_form.form_data.fields[1].value = submitted_form.password_value;
  submitted_form.preferred = true;
  EXPECT_TRUE(FormStructure(submitted_form.form_data).ShouldBeUploaded());

  form_manager()->ProvisionallySave(submitted_form);

  // The username and password fields contain stored values. This should be
  // signaled in the vote.
  std::map<base::string16, autofill::FieldPropertiesMask>
      expected_field_properties = {
          {submitted_form.username_element, FieldPropertiesFlags::KNOWN_VALUE},
          {submitted_form.password_element, 0}};

  // All votes should be FIRST_USE.
  std::map<base::string16, autofill::AutofillUploadContents::Field::VoteType>
      expected_vote_types = {
          {submitted_form.username_element,
           autofill::AutofillUploadContents::Field::FIRST_USE}};

  // Unrelated vote
  EXPECT_CALL(
      *client()->mock_driver()->mock_autofill_download_manager(),
      StartUploadRequest(SignatureIsSameAs(different_password), _, _, _, _, _))
      .Times(AtMost(1));
  // First login vote
  EXPECT_CALL(
      *client()->mock_driver()->mock_autofill_download_manager(),
      StartUploadRequest(
          AllOf(SignatureIsSameAs(submitted_form),
                UploadedAutofillTypesAre(FieldTypeMap(
                    {{submitted_form.username_element, autofill::USERNAME},
                     {submitted_form.password_element, autofill::UNKNOWN_TYPE},
                     {ASCIIToUTF16("petname"), autofill::UNKNOWN_TYPE}})),
                UploadedFieldPropertiesMasksAre(expected_field_properties),
                VoteTypesAre(expected_vote_types)),
          _, autofill::ServerFieldTypeSet({autofill::USERNAME}), _, true,
          nullptr));

  form_manager()->Save();
}

// Values on a submitted form should be marked as KNOWN_VALUE only if they match
// values from the credential which was used to log in. Other stored credentials
// are ignored.
TEST_F(PasswordFormManagerTest, FirstLoginVote_MatchOnlySubmittedCredentials) {
  PasswordForm alternative_credential = *saved_match();
  alternative_credential.username_value = ASCIIToUTF16("flatmate");
  alternative_credential.password_value = ASCIIToUTF16("p@ssword");
  fake_form_fetcher()->SetNonFederated({&alternative_credential, saved_match()},
                                       0u);

  // User submits credentials for the observed form.
  PasswordForm submitted_form =
      CreateMinimalCrowdsourcableForm(*observed_form());
  submitted_form.username_value = saved_match()->username_value;
  submitted_form.password_value = saved_match()->password_value;
  submitted_form.form_data.fields[0].value = submitted_form.username_value;
  submitted_form.form_data.fields[1].value = submitted_form.password_value;
  // Use a value from an alternative credential. It should not be voted as a
  // known value.
  submitted_form.form_data.fields[2].value =
      alternative_credential.username_value;
  submitted_form.preferred = true;
  EXPECT_TRUE(FormStructure(submitted_form.form_data).ShouldBeUploaded());

  form_manager()->ProvisionallySave(submitted_form);

  // The username and password fields contain stored values. This should be
  // signaled in the vote.
  std::map<base::string16, autofill::FieldPropertiesMask>
      expected_field_properties = {
          {submitted_form.username_element, FieldPropertiesFlags::KNOWN_VALUE},
          {submitted_form.password_element, FieldPropertiesFlags::KNOWN_VALUE},
          {ASCIIToUTF16("petname"), 0}};

  // All votes should be FIRST_USE.
  std::map<base::string16, autofill::AutofillUploadContents::Field::VoteType>
      expected_vote_types = {
          {submitted_form.username_element,
           autofill::AutofillUploadContents::Field::FIRST_USE},
          {submitted_form.password_element,
           autofill::AutofillUploadContents::Field::FIRST_USE}};

  // Unrelated vote.
  EXPECT_CALL(*client()->mock_driver()->mock_autofill_download_manager(),
              StartUploadRequest(SignatureIsSameAs(*saved_match()), _, _, _, _,
                                 nullptr))
      .Times(1);
  // First login vote.
  EXPECT_CALL(
      *client()->mock_driver()->mock_autofill_download_manager(),
      StartUploadRequest(
          AllOf(SignatureIsSameAs(submitted_form),
                UploadedAutofillTypesAre(FieldTypeMap(
                    {{submitted_form.username_element, autofill::USERNAME},
                     {submitted_form.password_element, autofill::PASSWORD},
                     {ASCIIToUTF16("petname"), autofill::UNKNOWN_TYPE}})),
                UploadedFieldPropertiesMasksAre(expected_field_properties),
                VoteTypesAre(expected_vote_types)),
          _,
          autofill::ServerFieldTypeSet(
              {autofill::USERNAME, autofill::PASSWORD}),
          _, true, nullptr));

  form_manager()->Save();
}

TEST_F(PasswordFormManagerTest, FirstLoginVote_NoUsernameSaved) {
  // We have a credential without a username saved.
  saved_match()->username_element.clear();
  saved_match()->username_value.clear();
  fake_form_fetcher()->SetNonFederated({saved_match()}, 0u);

  // User submits credentials for the observed form.
  PasswordForm submitted_form =
      CreateMinimalCrowdsourcableForm(*observed_form());
  submitted_form.username_value = saved_match()->username_value;
  submitted_form.password_value = saved_match()->password_value;
  submitted_form.form_data.fields[0].value = submitted_form.username_value;
  submitted_form.form_data.fields[1].value = submitted_form.password_value;
  // An empty field should not be a known username, even if we have no username.
  submitted_form.form_data.fields[2].value.clear();
  submitted_form.preferred = true;
  EXPECT_TRUE(FormStructure(submitted_form.form_data).ShouldBeUploaded());

  form_manager()->ProvisionallySave(submitted_form);

  // The password field contains stored values. This should be signaled in the
  // vote.
  std::map<base::string16, autofill::FieldPropertiesMask>
      expected_field_properties = {
          {submitted_form.username_element, 0},  // Don't match empty values.
          {submitted_form.password_element, FieldPropertiesFlags::KNOWN_VALUE}};

  // All votes should be FIRST_USE.
  std::map<base::string16, autofill::AutofillUploadContents::Field::VoteType>
      expected_vote_types = {
          {submitted_form.username_element,
           autofill::AutofillUploadContents::Field::FIRST_USE},
          {submitted_form.password_element,
           autofill::AutofillUploadContents::Field::FIRST_USE},
          {ASCIIToUTF16("petname"),
           autofill::AutofillUploadContents::Field::NO_INFORMATION}};

  // Unrelated vote.
  EXPECT_CALL(*client()->mock_driver()->mock_autofill_download_manager(),
              StartUploadRequest(SignatureIsSameAs(*saved_match()), _, _, _, _,
                                 nullptr))
      .Times(1);
  // First login vote.
  EXPECT_CALL(
      *client()->mock_driver()->mock_autofill_download_manager(),
      StartUploadRequest(
          AllOf(SignatureIsSameAs(submitted_form),
                UploadedAutofillTypesAre(FieldTypeMap(
                    {{submitted_form.username_element, autofill::USERNAME},
                     {submitted_form.password_element, autofill::PASSWORD},
                     {ASCIIToUTF16("petname"), autofill::UNKNOWN_TYPE}})),
                UploadedFieldPropertiesMasksAre(expected_field_properties),
                VoteTypesAre(expected_vote_types)),
          _,
          autofill::ServerFieldTypeSet(
              {autofill::USERNAME, autofill::PASSWORD}),
          _, true, nullptr));

  form_manager()->Save();
}

// Upload a first login (i.e. first use) vote when the form has no username
// field.
TEST_F(PasswordFormManagerTest, FirstLoginVote_NoUsernameSubmitted) {
  fake_form_fetcher()->SetNonFederated({saved_match()}, 0u);

  // User submits credentials for the observed form.
  PasswordForm submitted_form = *observed_form();
  submitted_form.origin = GURL("https://www.foo.com/login");
  submitted_form.form_data.origin = submitted_form.origin;

  autofill::FormFieldData field;
  field.name = ASCIIToUTF16("password1");
  field.form_control_type = "password";
  submitted_form.form_data.fields.push_back(field);
  field.name = ASCIIToUTF16("password2");
  field.form_control_type = "new-password";
  submitted_form.form_data.fields.push_back(field);
  field.name = ASCIIToUTF16("password3");
  field.form_control_type = "new-password";
  submitted_form.form_data.fields.push_back(field);

  submitted_form.username_value.clear();
  submitted_form.username_element.clear();
  submitted_form.password_value = saved_match()->password_value;
  submitted_form.password_element = ASCIIToUTF16("password1");
  submitted_form.form_data.fields[0].value = saved_match()->password_value;
  submitted_form.form_data.fields[1].value = ASCIIToUTF16("newpassword");
  submitted_form.form_data.fields[2].value = ASCIIToUTF16("newpassword");
  submitted_form.preferred = true;
  EXPECT_TRUE(FormStructure(submitted_form.form_data).ShouldBeUploaded());

  form_manager()->ProvisionallySave(submitted_form);

  std::map<base::string16, autofill::FieldPropertiesMask>
      expected_field_properties = {
          {submitted_form.password_element, FieldPropertiesFlags::KNOWN_VALUE}};

  std::map<base::string16, autofill::AutofillUploadContents::Field::VoteType>
      expected_vote_types = {
          {submitted_form.password_element,
           autofill::AutofillUploadContents::Field::FIRST_USE}};

  FieldTypeMap expected_votes = {
      {submitted_form.form_data.fields[0].name, autofill::PASSWORD},
      {submitted_form.form_data.fields[1].name, autofill::UNKNOWN_TYPE},
      {submitted_form.form_data.fields[2].name, autofill::UNKNOWN_TYPE}};

  // Unrelated vote.
  EXPECT_CALL(*client()->mock_driver()->mock_autofill_download_manager(),
              StartUploadRequest(SignatureIsSameAs(*saved_match()), _, _, _, _,
                                 nullptr))
      .Times(1);
  // First login vote.
  EXPECT_CALL(
      *client()->mock_driver()->mock_autofill_download_manager(),
      StartUploadRequest(
          AllOf(SignatureIsSameAs(submitted_form),
                UploadedAutofillTypesAre(expected_votes),
                UploadedFieldPropertiesMasksAre(expected_field_properties),
                VoteTypesAre(expected_vote_types)),
          _, autofill::ServerFieldTypeSet({autofill::PASSWORD}), _, true,
          nullptr));

  form_manager()->Save();
}

// All fields with a known value should have the KNOWN_VALUE flag.
TEST_F(PasswordFormManagerTest, FirstLoginVote_KnownValue) {
  fake_form_fetcher()->SetNonFederated({saved_match()}, 0u);

  autofill::FormFieldData field;
  field.name = ASCIIToUTF16("email");
  field.form_control_type = "text";
  observed_form()->form_data.fields.push_back(field);
  field.name = ASCIIToUTF16("petname");
  field.form_control_type = "text";
  observed_form()->form_data.fields.push_back(field);
  field.name = ASCIIToUTF16("pin");
  field.form_control_type = "password";
  observed_form()->form_data.fields.push_back(field);
  field.name = ASCIIToUTF16("password");
  field.form_control_type = "password";
  observed_form()->form_data.fields.push_back(field);
  field.name = ASCIIToUTF16("repeat password");
  field.form_control_type = "password";
  observed_form()->form_data.fields.push_back(field);
  field.name = ASCIIToUTF16("repeat email");
  field.form_control_type = "text";
  observed_form()->form_data.fields.push_back(field);
  field.name = ASCIIToUTF16("empty password");
  field.form_control_type = "password";
  observed_form()->form_data.fields.push_back(field);
  field.name = ASCIIToUTF16("empty text");
  field.form_control_type = "text";
  observed_form()->form_data.fields.push_back(field);
  observed_form()->username_element = ASCIIToUTF16("email");
  observed_form()->password_element = ASCIIToUTF16("password");
  // User submits credentials for the observed form.
  PasswordForm submitted_form = *observed_form();
  submitted_form.origin = GURL("https://www.foo.com/login");
  submitted_form.form_data.origin = submitted_form.origin;
  submitted_form.username_value = saved_match()->username_value;
  submitted_form.password_value = saved_match()->password_value;
  submitted_form.form_data.fields[0].value = submitted_form.username_value;
  submitted_form.form_data.fields[1].value = ASCIIToUTF16("Snoop");
  submitted_form.form_data.fields[2].value = ASCIIToUTF16("1234");
  submitted_form.form_data.fields[3].value = submitted_form.password_value;
  submitted_form.form_data.fields[4].value = submitted_form.password_value;
  submitted_form.form_data.fields[5].value = submitted_form.username_value;
  submitted_form.form_data.fields[6].value.clear();
  submitted_form.form_data.fields[7].value.clear();
  submitted_form.preferred = true;
  EXPECT_TRUE(FormStructure(submitted_form.form_data).ShouldBeUploaded());

  form_manager()->ProvisionallySave(submitted_form);

  std::map<base::string16, autofill::FieldPropertiesMask>
      expected_field_properties = {
          {submitted_form.form_data.fields[0].name,
           FieldPropertiesFlags::KNOWN_VALUE},
          {submitted_form.form_data.fields[1].name, 0},
          {submitted_form.form_data.fields[2].name, 0},
          {submitted_form.form_data.fields[3].name,
           FieldPropertiesFlags::KNOWN_VALUE},
          {submitted_form.form_data.fields[4].name,
           FieldPropertiesFlags::KNOWN_VALUE},
          {submitted_form.form_data.fields[5].name,
           FieldPropertiesFlags::KNOWN_VALUE},
          {submitted_form.form_data.fields[6].name, 0},
          {submitted_form.form_data.fields[7].name, 0}};

  // Only the detected username_element and password_element fields should have
  // a USERNAME and PASSWORD vote.
  std::map<base::string16, autofill::AutofillUploadContents::Field::VoteType>
      expected_vote_types = {
          {submitted_form.form_data.fields[0].name,
           autofill::AutofillUploadContents::Field::FIRST_USE},
          {submitted_form.form_data.fields[1].name,
           autofill::AutofillUploadContents::Field::NO_INFORMATION},
          {submitted_form.form_data.fields[2].name,
           autofill::AutofillUploadContents::Field::NO_INFORMATION},
          {submitted_form.form_data.fields[3].name,
           autofill::AutofillUploadContents::Field::FIRST_USE},
          {submitted_form.form_data.fields[4].name,
           autofill::AutofillUploadContents::Field::NO_INFORMATION},
          {submitted_form.form_data.fields[5].name,
           autofill::AutofillUploadContents::Field::NO_INFORMATION},
          {submitted_form.form_data.fields[6].name,
           autofill::AutofillUploadContents::Field::NO_INFORMATION},
          {submitted_form.form_data.fields[7].name,
           autofill::AutofillUploadContents::Field::NO_INFORMATION}};

  FieldTypeMap expected_votes = {
      {submitted_form.username_element, autofill::USERNAME},
      {submitted_form.form_data.fields[1].name, autofill::UNKNOWN_TYPE},
      {submitted_form.form_data.fields[2].name, autofill::UNKNOWN_TYPE},
      {submitted_form.password_element, autofill::PASSWORD},
      {submitted_form.form_data.fields[4].name, autofill::UNKNOWN_TYPE},
      {submitted_form.form_data.fields[5].name, autofill::UNKNOWN_TYPE},
      {submitted_form.form_data.fields[6].name, autofill::UNKNOWN_TYPE},
      {submitted_form.form_data.fields[7].name, autofill::UNKNOWN_TYPE}};

  // Unrelated vote.
  EXPECT_CALL(*client()->mock_driver()->mock_autofill_download_manager(),
              StartUploadRequest(SignatureIsSameAs(*saved_match()), _, _, _, _,
                                 nullptr))
      .Times(1);
  // First login vote.
  EXPECT_CALL(
      *client()->mock_driver()->mock_autofill_download_manager(),
      StartUploadRequest(
          AllOf(SignatureIsSameAs(submitted_form),
                UploadedAutofillTypesAre(expected_votes),
                UploadedFieldPropertiesMasksAre(expected_field_properties),
                VoteTypesAre(expected_vote_types)),
          _,
          autofill::ServerFieldTypeSet(
              {autofill::USERNAME, autofill::PASSWORD}),
          _, true, nullptr));

  form_manager()->Save();
}

TEST_F(PasswordFormManagerTest, UploadPasswordAttributesVote) {
  PasswordForm credentials = *observed_form();
  // Set FormData to enable crowdsourcing.
  credentials.form_data = saved_match()->form_data;
  FakeFormFetcher fetcher;
  PasswordFormManager form_manager(
      password_manager(), client(), client()->driver(), credentials,
      std::make_unique<NiceMock<MockFormSaver>>(), &fetcher);
  form_manager.Init(nullptr);
  fetcher.SetNonFederated(std::vector<const PasswordForm*>(), 0u);

  credentials.username_value = saved_match()->username_value;
  credentials.password_value = ASCIIToUTF16("12345");
  form_manager.ProvisionallySave(credentials);

  EXPECT_CALL(
      *client()->mock_driver()->mock_autofill_download_manager(),
      StartUploadRequest(HasPasswordAttributesVote(true /* is_vote_expected */),
                         _, _, _, _, nullptr));
  form_manager.Save();
}

TEST_F(PasswordFormManagerTest, PresaveGeneratedPassword_UnknownUsername) {
  // Checks whether the generated password is presaved with the captured
  // username. The username is new, so there will be no credentail override.
  fake_form_fetcher()->SetNonFederated({saved_match()}, 0u);
  PasswordForm credentials = *observed_form();
  credentials.username_value = ASCIIToUTF16("new_user");
  credentials.password_value = ASCIIToUTF16("generatated_password");

  EXPECT_CALL(MockFormSaver::Get(form_manager()),
              PresaveGeneratedPassword(credentials));
  form_manager()->PresaveGeneratedPassword(credentials);
}

TEST_F(PasswordFormManagerTest, PresaveGeneratedPassword_KnownUsername) {
  // Checks whether the generated password is presaved with empty username if
  // there is already an entry with the captured username in the store.
  PasswordForm saved_form_without_username(*saved_match());
  saved_form_without_username.username_value.clear();
  fake_form_fetcher()->SetNonFederated(
      {saved_match(), &saved_form_without_username}, 0u);
  PasswordForm credentials = *observed_form();
  credentials.username_value = saved_match()->username_value;
  credentials.password_value = ASCIIToUTF16("generatated_password");

  PasswordForm credentials_without_username(credentials);
  credentials_without_username.username_value.clear();
  EXPECT_CALL(MockFormSaver::Get(form_manager()),
              PresaveGeneratedPassword(credentials_without_username));
  form_manager()->PresaveGeneratedPassword(credentials);
}

TEST_F(PasswordFormManagerTest, PresaveGeneratedPassword_EmptyUsername) {
  // Checks whether the generated password is presaved even if the captured
  // username is empty.
  PasswordForm saved_form_without_username(*saved_match());
  saved_form_without_username.username_value.clear();
  fake_form_fetcher()->SetNonFederated(
      {saved_match(), &saved_form_without_username}, 0u);
  PasswordForm credentials = *observed_form();
  credentials.username_value.clear();
  credentials.password_value = ASCIIToUTF16("generatated_password");

  EXPECT_CALL(MockFormSaver::Get(form_manager()),
              PresaveGeneratedPassword(credentials));
  form_manager()->PresaveGeneratedPassword(credentials);
}

TEST_F(PasswordFormManagerTest, MetricForManuallyTypedAndGeneratedPasswords) {
  for (bool is_generated_password : {false, true}) {
    SCOPED_TRACE(testing::Message("is_generated_password = ")
                 << is_generated_password);
    fake_form_fetcher()->SetNonFederated(std::vector<const PasswordForm*>(),
                                         0u);

    PasswordForm credentials(*observed_form());
    credentials.username_value = ASCIIToUTF16("test@gmail.com");
    credentials.preferred = true;
    if (is_generated_password) {
      credentials.new_password_value = ASCIIToUTF16("12345");
      form_manager()->PresaveGeneratedPassword(credentials);
    }
    form_manager()->ProvisionallySave(credentials);

    PasswordForm saved_result;
    EXPECT_CALL(MockFormSaver::Get(form_manager()), Save(_, _));
    base::HistogramTester histogram_tester;
    form_manager()->Save();
    histogram_tester.ExpectUniqueSample(
        "PasswordManager.NewlySavedPasswordIsGenerated", is_generated_password,
        1);
  }
}

TEST_F(PasswordFormManagerTest, UserEventsForGeneration) {
  using GeneratedPasswordStatus =
      PasswordFormMetricsRecorder::GeneratedPasswordStatus;

  PasswordForm submitted_form(*observed_form());
  submitted_form.username_value = ASCIIToUTF16("username");
  submitted_form.password_value = ASCIIToUTF16("123456");

  {  // User accepts a generated password.
    base::HistogramTester histogram_tester;
    {
      FakeFormFetcher fetcher;
      PasswordFormManager form_manager(
          password_manager(), client(), client()->driver(), *observed_form(),
          std::make_unique<NiceMock<MockFormSaver>>(), &fetcher);
      form_manager.Init(nullptr);
      form_manager.PresaveGeneratedPassword(submitted_form);
    }
    histogram_tester.ExpectUniqueSample(
        "PasswordGeneration.UserDecision",
        GeneratedPasswordStatus::kPasswordAccepted, 1);
  }

  {  // User edits the generated password.
    base::HistogramTester histogram_tester;
    {
      FakeFormFetcher fetcher;
      PasswordFormManager form_manager(
          password_manager(), client(), client()->driver(), *observed_form(),
          std::make_unique<NiceMock<MockFormSaver>>(), &fetcher);
      form_manager.Init(nullptr);
      form_manager.PresaveGeneratedPassword(submitted_form);
      submitted_form.password_value = ASCIIToUTF16("password123456");
      form_manager.PresaveGeneratedPassword(submitted_form);
    }
    histogram_tester.ExpectUniqueSample(
        "PasswordGeneration.UserDecision",
        GeneratedPasswordStatus::kPasswordEdited, 1);
  }

  {  // User clears the generated password.
    base::HistogramTester histogram_tester;
    {
      FakeFormFetcher fetcher;
      PasswordFormManager form_manager(
          password_manager(), client(), client()->driver(), *observed_form(),
          std::make_unique<NiceMock<MockFormSaver>>(), &fetcher);
      form_manager.Init(nullptr);
      form_manager.PresaveGeneratedPassword(submitted_form);
      submitted_form.password_value = ASCIIToUTF16("password123");
      form_manager.PresaveGeneratedPassword(submitted_form);
      form_manager.PasswordNoLongerGenerated();
    }
    histogram_tester.ExpectUniqueSample(
        "PasswordGeneration.UserDecision",
        GeneratedPasswordStatus::kPasswordDeleted, 1);
  }
}

}  // namespace password_manager
