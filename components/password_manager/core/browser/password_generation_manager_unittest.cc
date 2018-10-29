// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_generation_manager.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/message_loop/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/password_requirements_spec_fetcher.h"
#include "components/autofill/core/browser/proto/password_requirements.pb.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/password_form_generation_data.h"
#include "components/autofill/core/common/signatures_util.h"
#include "components/password_manager/core/browser/password_autofill_manager.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_requirements_service.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/entropy_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using autofill::FormStructure;
using base::ASCIIToUTF16;
using testing::_;

namespace password_manager {

namespace {

// Magic constants.
// Host (suffix) of a server for which no domain wide requirements are given.
constexpr char kNoServerResponse[] = "www.no_server_response.com";
// Host (suffix) of a server for which the autofill requirements are overriden
// via a domain wide spec.
constexpr char kHasServerResponse[] = "www.has_server_response.com";

class TestPasswordManagerDriver : public StubPasswordManagerDriver {
 public:
  explicit TestPasswordManagerDriver(PasswordManagerClient* client)
      : password_manager_(client),
        password_generation_manager_(client, this),
        password_autofill_manager_(this, nullptr, client) {}
  ~TestPasswordManagerDriver() override {}

  // PasswordManagerDriver implementation.
  PasswordGenerationManager* GetPasswordGenerationManager() override {
    return &password_generation_manager_;
  }
  PasswordManager* GetPasswordManager() override { return &password_manager_; }
  PasswordAutofillManager* GetPasswordAutofillManager() override {
    return &password_autofill_manager_;
  }
  void FormsEligibleForGenerationFound(
      const std::vector<autofill::PasswordFormGenerationData>& forms) override {
    found_forms_eligible_for_generation_.insert(
        found_forms_eligible_for_generation_.begin(), forms.begin(),
        forms.end());
  }

  const std::vector<autofill::PasswordFormGenerationData>&
  GetFoundEligibleForGenerationForms() {
    return found_forms_eligible_for_generation_;
  }

  MOCK_METHOD0(AllowToRunFormClassifier, void());

 private:
  PasswordManager password_manager_;
  PasswordGenerationManager password_generation_manager_;
  PasswordAutofillManager password_autofill_manager_;
  std::vector<autofill::PasswordFormGenerationData>
      found_forms_eligible_for_generation_;
};

autofill::PasswordRequirementsSpec GetDomainWideRequirements() {
  autofill::PasswordRequirementsSpec spec;
  spec.set_max_length(7);
  spec.set_priority(20);
  return spec;
}

autofill::PasswordRequirementsSpec GetFieldRequirements() {
  autofill::PasswordRequirementsSpec spec;
  spec.set_max_length(8);
  spec.set_priority(10);
  return spec;
}

class FakePasswordRequirementsSpecFetcher
    : public autofill::PasswordRequirementsSpecFetcher {
 public:
  using FetchCallback =
      autofill::PasswordRequirementsSpecFetcher::FetchCallback;

  FakePasswordRequirementsSpecFetcher() = default;
  ~FakePasswordRequirementsSpecFetcher() override = default;

  void Fetch(GURL origin, FetchCallback callback) override {
    if (origin.GetOrigin().host_piece().find(kNoServerResponse) !=
        std::string::npos) {
      std::move(callback).Run(autofill::PasswordRequirementsSpec());
    } else if (origin.GetOrigin().host_piece().find(kHasServerResponse) !=
               std::string::npos) {
      std::move(callback).Run(GetDomainWideRequirements());
    } else {
      NOTREACHED();
    }
  }
};

class MockPasswordManagerClient : public StubPasswordManagerClient {
 public:
  MOCK_CONST_METHOD0(GetPasswordSyncState, SyncState());
  MOCK_CONST_METHOD0(IsSavingAndFillingEnabledForCurrentPage, bool());
  MOCK_CONST_METHOD0(IsIncognito, bool());

  explicit MockPasswordManagerClient(std::unique_ptr<PrefService> prefs)
      : prefs_(std::move(prefs)),
        store_(new TestPasswordStore),
        driver_(this),
        password_requirements_service_(
            std::make_unique<FakePasswordRequirementsSpecFetcher>()) {}

  ~MockPasswordManagerClient() override { store_->ShutdownOnUIThread(); }

  PasswordStore* GetPasswordStore() const override { return store_.get(); }
  PrefService* GetPrefs() const override { return prefs_.get(); }
  PasswordRequirementsService* GetPasswordRequirementsService() override {
    return &password_requirements_service_;
  }
  void SetLastCommittedEntryUrl(const GURL& url) { last_committed_url_ = url; }
  const GURL& GetLastCommittedEntryURL() const override {
    return last_committed_url_;
  }

  TestPasswordManagerDriver* test_driver() { return &driver_; }

 private:
  std::unique_ptr<PrefService> prefs_;
  scoped_refptr<TestPasswordStore> store_;
  TestPasswordManagerDriver driver_;
  PasswordRequirementsService password_requirements_service_;
  GURL last_committed_url_;
};

}  // anonymous namespace

class PasswordGenerationManagerTest : public testing::Test {
 protected:
  void SetUp() override {
    // Construct a PrefService and register all necessary prefs before handing
    // it off to |client_|, as the initialization flow of |client_| will
    // indirectly cause those prefs to be immediately accessed.
    std::unique_ptr<TestingPrefServiceSimple> prefs(
        new TestingPrefServiceSimple());
    prefs->registry()->RegisterBooleanPref(prefs::kCredentialsEnableService,
                                           true);
    client_.reset(new MockPasswordManagerClient(std::move(prefs)));
  }

  void TearDown() override { client_.reset(); }

  PasswordGenerationManager* GetGenerationManager() {
    return client_->test_driver()->GetPasswordGenerationManager();
  }

  TestPasswordManagerDriver* GetTestDriver() { return client_->test_driver(); }

  bool IsGenerationEnabled() {
    return GetGenerationManager()->IsGenerationEnabled(true);
  }

  void DetectFormsEligibleForGeneration(
      const std::vector<autofill::FormStructure*>& forms) {
    GetGenerationManager()->DetectFormsEligibleForGeneration(forms);
  }

  base::MessageLoop message_loop_;
  std::unique_ptr<MockPasswordManagerClient> client_;
};

TEST_F(PasswordGenerationManagerTest, IsGenerationEnabled) {
  // Enabling the PasswordManager and password sync should cause generation to
  // be enabled, unless the sync is with a custom passphrase.
  EXPECT_CALL(*client_, IsSavingAndFillingEnabledForCurrentPage())
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*client_, GetPasswordSyncState())
      .WillRepeatedly(testing::Return(SYNCING_NORMAL_ENCRYPTION));
  EXPECT_TRUE(IsGenerationEnabled());

  EXPECT_CALL(*client_, GetPasswordSyncState())
      .WillRepeatedly(testing::Return(SYNCING_WITH_CUSTOM_PASSPHRASE));
  EXPECT_TRUE(IsGenerationEnabled());

  // Disabling password syncing should cause generation to be disabled.
  EXPECT_CALL(*client_, GetPasswordSyncState())
      .WillRepeatedly(testing::Return(NOT_SYNCING));
  EXPECT_FALSE(IsGenerationEnabled());

  // Disabling the PasswordManager should cause generation to be disabled even
  // if syncing is enabled.
  EXPECT_CALL(*client_, IsSavingAndFillingEnabledForCurrentPage())
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(*client_, GetPasswordSyncState())
      .WillRepeatedly(testing::Return(SYNCING_NORMAL_ENCRYPTION));
  EXPECT_FALSE(IsGenerationEnabled());

  EXPECT_CALL(*client_, GetPasswordSyncState())
      .WillRepeatedly(testing::Return(SYNCING_WITH_CUSTOM_PASSPHRASE));
  EXPECT_FALSE(IsGenerationEnabled());
}

// Verify that password requirements received from the autofill server are
// stored and that domain-wide password requirements are fetched as well.
TEST_F(PasswordGenerationManagerTest, ProcessPasswordRequirements) {
  // Setup so that IsGenerationEnabled() returns true.
  EXPECT_CALL(*client_, IsSavingAndFillingEnabledForCurrentPage())
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*client_, GetPasswordSyncState())
      .WillRepeatedly(testing::Return(SYNCING_NORMAL_ENCRYPTION));
  struct {
    const char* name;
    bool has_domain_wide_requirements = false;
    bool has_field_requirements = false;
    autofill::PasswordRequirementsSpec expected_spec;
  } kTests[] = {
      {
          .name = "No known requirements",
          .expected_spec = autofill::PasswordRequirementsSpec(),
      },
      {
          .name = "Only domain wide requirements",
          .has_domain_wide_requirements = true,
          .expected_spec = GetDomainWideRequirements(),
      },
      {
          .name = "Only field requirements",
          .has_field_requirements = true,
          .expected_spec = GetFieldRequirements(),
      },
      {
          .name = "Domain wide requirements take precedence",
          .has_domain_wide_requirements = true,
          .has_field_requirements = true,
          .expected_spec = GetDomainWideRequirements(),
      },
  };

  // The purpose of this counter is to generate unique URLs and field signatures
  // so that no caching can happen between test runs. If this causes issues in
  // the future, the test should be converted into a parameterized test that
  // creates unqieue PasswordRequirementsService instances for each run.
  int test_counter = 0;
  for (const auto& test : kTests) {
    SCOPED_TRACE(test.name);
    ++test_counter;

    autofill::FormFieldData username;
    username.label = ASCIIToUTF16("username");
    username.name = ASCIIToUTF16("login");
    username.form_control_type = "text";

    autofill::FormFieldData password;
    password.label = ASCIIToUTF16("password");
    password.name =
        ASCIIToUTF16(base::StringPrintf("password%d", test_counter));
    password.form_control_type = "password";

    autofill::FormData account_creation_form;
    account_creation_form.origin = GURL("http://accounts.yahoo.com/");
    account_creation_form.action = GURL("http://accounts.yahoo.com/signup");
    account_creation_form.name = ASCIIToUTF16("account_creation_form");
    account_creation_form.fields.push_back(username);
    account_creation_form.fields.push_back(password);

    autofill::FormStructure form(account_creation_form);

    std::vector<autofill::FormStructure*> forms = {&form};

    // EMAIL_ADDRESS = 9
    // ACCOUNT_CREATION_PASSWORD = 76
    autofill::AutofillQueryResponseContents response;
    response.add_field()->set_overall_type_prediction(9);
    response.add_field()->set_overall_type_prediction(76);

    if (test.has_field_requirements) {
      *response.mutable_field(1)->mutable_password_requirements() =
          GetFieldRequirements();
    }
    // Configure the last committed entry URL with some magic constants for
    // which the FakePasswordRequirementsFetcher is configured to respond
    // with a filled or empty response.
    GURL origin(base::StringPrintf("https://%d-%s/", test_counter,
                                   test.has_domain_wide_requirements
                                       ? kHasServerResponse
                                       : kNoServerResponse));
    client_->SetLastCommittedEntryUrl(origin);

    std::string response_string;
    ASSERT_TRUE(response.SerializeToString(&response_string));
    autofill::FormStructure::ParseQueryResponse(response_string, forms,
                                                nullptr);

    GetGenerationManager()->PrefetchSpec(origin.GetOrigin());

    // Processs the password requirements with expected side effects of
    // either storing the requirements from the AutofillQueryResponseContents)
    // in the PasswordRequirementsService.
    GetGenerationManager()->ProcessPasswordRequirements(forms);

    // Validate the result.
    autofill::FormSignature form_signature =
        autofill::CalculateFormSignature(account_creation_form);
    autofill::FieldSignature field_signature =
        autofill::CalculateFieldSignatureForField(password);
    autofill::PasswordRequirementsSpec spec =
        client_->GetPasswordRequirementsService()->GetSpec(
            origin, form_signature, field_signature);
    EXPECT_EQ(test.expected_spec.max_length(), spec.max_length());
  }
}

TEST_F(PasswordGenerationManagerTest, DetectFormsEligibleForGeneration) {
  // Setup so that IsGenerationEnabled() returns true.
  EXPECT_CALL(*client_, IsSavingAndFillingEnabledForCurrentPage())
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*client_, GetPasswordSyncState())
      .WillRepeatedly(testing::Return(SYNCING_NORMAL_ENCRYPTION));

  autofill::FormData login_form;
  login_form.origin = GURL("http://www.yahoo.com/login/");
  autofill::FormFieldData username;
  username.label = ASCIIToUTF16("username");
  username.name = ASCIIToUTF16("login");
  username.form_control_type = "text";
  login_form.fields.push_back(username);
  autofill::FormFieldData password;
  password.label = ASCIIToUTF16("password");
  password.name = ASCIIToUTF16("password");
  password.form_control_type = "password";
  login_form.fields.push_back(password);
  autofill::FormStructure form1(login_form);
  std::vector<autofill::FormStructure*> forms;
  forms.push_back(&form1);

  autofill::FormData account_creation_form;
  account_creation_form.origin = GURL("http://accounts.yahoo.com/");
  account_creation_form.action = GURL("http://accounts.yahoo.com/signup");
  account_creation_form.name = ASCIIToUTF16("account_creation_form");
  account_creation_form.fields.push_back(username);
  account_creation_form.fields.push_back(password);
  autofill::FormFieldData confirm_password;
  confirm_password.label = ASCIIToUTF16("confirm_password");
  confirm_password.name = ASCIIToUTF16("confirm_password");
  confirm_password.form_control_type = "password";
  account_creation_form.fields.push_back(confirm_password);
  autofill::FormSignature account_creation_form_signature =
      autofill::CalculateFormSignature(account_creation_form);
  autofill::FieldSignature account_creation_field_signature =
      autofill::CalculateFieldSignatureForField(password);
  autofill::FieldSignature confirmation_field_signature =
      autofill::CalculateFieldSignatureForField(confirm_password);
  autofill::FormStructure form2(account_creation_form);
  forms.push_back(&form2);

  autofill::FormData change_password_form;
  change_password_form.origin = GURL("http://accounts.yahoo.com/");
  change_password_form.action = GURL("http://accounts.yahoo.com/change");
  change_password_form.name = ASCIIToUTF16("change_password_form");
  change_password_form.fields.push_back(password);
  change_password_form.fields[0].name = ASCIIToUTF16("new_password");
  change_password_form.fields.push_back(confirm_password);
  autofill::FormStructure form3(change_password_form);
  autofill::FormSignature change_password_form_signature =
      autofill::CalculateFormSignature(change_password_form);
  autofill::FieldSignature change_password_field_signature =
      autofill::CalculateFieldSignatureForField(change_password_form.fields[0]);
  forms.push_back(&form3);

  // Simulate the server response to set the field types.
  // The server response numbers mean:
  // EMAIL_ADDRESS = 9
  // PASSWORD = 75
  // ACCOUNT_CREATION_PASSWORD = 76
  // NEW_PASSWORD = 88
  // CONFIRMATION_PASSWORD = 95
  autofill::AutofillQueryResponseContents response;
  response.add_field()->set_overall_type_prediction(9);
  response.add_field()->set_overall_type_prediction(75);
  response.add_field()->set_overall_type_prediction(9);
  response.add_field()->set_overall_type_prediction(76);
  response.add_field()->set_overall_type_prediction(75);
  response.add_field()->set_overall_type_prediction(88);
  response.add_field()->set_overall_type_prediction(95);

  std::string response_string;
  ASSERT_TRUE(response.SerializeToString(&response_string));
  autofill::FormStructure::ParseQueryResponse(response_string, forms, nullptr);

  DetectFormsEligibleForGeneration(forms);
  EXPECT_EQ(2u, GetTestDriver()->GetFoundEligibleForGenerationForms().size());
  EXPECT_EQ(
      account_creation_form_signature,
      GetTestDriver()->GetFoundEligibleForGenerationForms()[0].form_signature);
  EXPECT_EQ(
      account_creation_field_signature,
      GetTestDriver()->GetFoundEligibleForGenerationForms()[0].field_signature);
  EXPECT_FALSE(GetTestDriver()
                   ->GetFoundEligibleForGenerationForms()[0]
                   .confirmation_field_signature.has_value());

  EXPECT_EQ(
      change_password_form_signature,
      GetTestDriver()->GetFoundEligibleForGenerationForms()[1].form_signature);
  EXPECT_EQ(
      change_password_field_signature,
      GetTestDriver()->GetFoundEligibleForGenerationForms()[1].field_signature);
  ASSERT_TRUE(GetTestDriver()
                  ->GetFoundEligibleForGenerationForms()[1]
                  .confirmation_field_signature.has_value());
  EXPECT_EQ(confirmation_field_signature,
            GetTestDriver()
                ->GetFoundEligibleForGenerationForms()[1]
                .confirmation_field_signature.value());
}

TEST_F(PasswordGenerationManagerTest, UpdatePasswordSyncStateIncognito) {
  // Disable password manager by going incognito. Even though password
  // syncing is enabled, generation should still be disabled.
  EXPECT_CALL(*client_, IsSavingAndFillingEnabledForCurrentPage())
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(*client_, IsIncognito()).WillRepeatedly(testing::Return(true));
  PrefService* prefs = client_->GetPrefs();
  prefs->SetBoolean(prefs::kCredentialsEnableService, true);
  EXPECT_CALL(*client_, GetPasswordSyncState())
      .WillRepeatedly(testing::Return(SYNCING_NORMAL_ENCRYPTION));

  EXPECT_FALSE(IsGenerationEnabled());
}

}  // namespace password_manager
