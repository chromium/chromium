// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_generation_frame_helper.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/metrics/field_trial.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/proto/password_requirements.pb.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/password_form_generation_data.h"
#include "components/autofill/core/common/signatures_util.h"
#include "components/password_manager/core/browser/generation/password_requirements_spec_fetcher.h"
#include "components/password_manager/core/browser/password_autofill_manager.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_requirements_service.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using autofill::FormStructure;
using autofill::PasswordRequirementsSpec;
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
        password_autofill_manager_(this, nullptr, client) {
    ON_CALL(*this, GetLastCommittedURL())
        .WillByDefault(testing::ReturnRef(empty_url_));
  }
  ~TestPasswordManagerDriver() override {}

  // PasswordManagerDriver implementation.
  PasswordGenerationFrameHelper* GetPasswordGenerationHelper() override {
    return &password_generation_manager_;
  }
  PasswordManager* GetPasswordManager() override { return &password_manager_; }
  PasswordAutofillManager* GetPasswordAutofillManager() override {
    return &password_autofill_manager_;
  }

  MOCK_METHOD0(AllowToRunFormClassifier, void());
  MOCK_CONST_METHOD0(GetLastCommittedURL, GURL&());

 private:
  GURL empty_url_;
  PasswordManager password_manager_;
  PasswordGenerationFrameHelper password_generation_manager_;
  PasswordAutofillManager password_autofill_manager_;
};

PasswordRequirementsSpec GetDomainWideRequirements() {
  PasswordRequirementsSpec spec;
  spec.set_max_length(7);
  spec.set_priority(20);
  return spec;
}

PasswordRequirementsSpec GetFieldRequirements() {
  PasswordRequirementsSpec spec;
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
      std::move(callback).Run(PasswordRequirementsSpec());
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
  MOCK_CONST_METHOD1(IsSavingAndFillingEnabled, bool(const GURL&));
  MOCK_CONST_METHOD0(IsIncognito, bool());

  explicit MockPasswordManagerClient(std::unique_ptr<PrefService> prefs)
      : prefs_(std::move(prefs)),
        store_(new TestPasswordStore),
        driver_(this),
        password_requirements_service_(
            std::make_unique<FakePasswordRequirementsSpecFetcher>()) {}

  ~MockPasswordManagerClient() override { store_->ShutdownOnUIThread(); }

  PasswordStore* GetProfilePasswordStore() const override {
    return store_.get();
  }
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

class PasswordGenerationFrameHelperTest : public testing::Test {
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

  PasswordGenerationFrameHelper* GetGenerationHelper() {
    return client_->test_driver()->GetPasswordGenerationHelper();
  }

  TestPasswordManagerDriver* GetTestDriver() { return client_->test_driver(); }

  bool IsGenerationEnabled() {
    return GetGenerationHelper()->IsGenerationEnabled(true);
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<MockPasswordManagerClient> client_;
};

TEST_F(PasswordGenerationFrameHelperTest, IsGenerationEnabled) {
  // Enabling the PasswordManager and password sync should cause generation to
  // be enabled, unless the sync is with a custom passphrase.
  EXPECT_CALL(*client_, IsSavingAndFillingEnabled(_))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*client_->GetMockPasswordFeatureManager(), IsGenerationEnabled())
      .WillRepeatedly(testing::Return(true));
  EXPECT_TRUE(IsGenerationEnabled());

  // Disabling password syncing should cause generation to be disabled.
  EXPECT_CALL(*client_->GetMockPasswordFeatureManager(), IsGenerationEnabled())
      .WillRepeatedly(testing::Return(false));
  EXPECT_FALSE(IsGenerationEnabled());

  // Disabling the PasswordManager should cause generation to be disabled even
  // if syncing is enabled.
  EXPECT_CALL(*client_, IsSavingAndFillingEnabled(_))
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(*client_->GetMockPasswordFeatureManager(), IsGenerationEnabled())
      .WillRepeatedly(testing::Return(true));
  EXPECT_FALSE(IsGenerationEnabled());
}

// Verify that password requirements received from the autofill server are
// stored and that domain-wide password requirements are fetched as well.
TEST_F(PasswordGenerationFrameHelperTest, ProcessPasswordRequirements) {
  // Setup so that IsGenerationEnabled() returns true.
  EXPECT_CALL(*client_, IsSavingAndFillingEnabled(_))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*client_->GetMockPasswordFeatureManager(), IsGenerationEnabled())
      .WillRepeatedly(testing::Return(true));
  struct {
    const char* name;
    bool has_domain_wide_requirements = false;
    bool has_field_requirements = false;
    PasswordRequirementsSpec expected_spec;
    // Assuming that a second form existed on the page for which no
    // per-formsignature-requirements exists, this indicates the expected
    // requirements that Chrome should conclude.
    PasswordRequirementsSpec expected_spec_for_unknown_signature;
  } kTests[] = {
      {
          .name = "No known requirements",
          .expected_spec = PasswordRequirementsSpec(),
          .expected_spec_for_unknown_signature = PasswordRequirementsSpec(),
      },
      {
          .name = "Only domain wide requirements",
          .has_domain_wide_requirements = true,
          .expected_spec = GetDomainWideRequirements(),
          .expected_spec_for_unknown_signature = GetDomainWideRequirements(),
      },
      {
          .name = "Only field requirements",
          .has_field_requirements = true,
          .expected_spec = GetFieldRequirements(),
          .expected_spec_for_unknown_signature = GetFieldRequirements(),
      },
      {
          .name = "Domain wide requirements take precedence",
          .has_domain_wide_requirements = true,
          .has_field_requirements = true,
          .expected_spec = GetDomainWideRequirements(),
          .expected_spec_for_unknown_signature = GetDomainWideRequirements(),
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
    username.name = ASCIIToUTF16("login");
    username.form_control_type = "text";

    autofill::FormFieldData password;
    password.name =
        ASCIIToUTF16(base::StringPrintf("password%d", test_counter));
    password.form_control_type = "password";

    // Configure the last committed entry URL with some magic constants for
    // which the FakePasswordRequirementsFetcher is configured to respond
    // with a filled or empty response.
    GURL origin(base::StringPrintf("https://%d-%s/", test_counter,
                                   test.has_domain_wide_requirements
                                       ? kHasServerResponse
                                       : kNoServerResponse));

    autofill::FormData account_creation_form;
    account_creation_form.url = origin;
    account_creation_form.action = origin;
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

    client_->SetLastCommittedEntryUrl(origin);

    std::string response_string;
    ASSERT_TRUE(response.SerializeToString(&response_string));
    autofill::FormStructure::ParseQueryResponse(response_string, forms,
                                                nullptr);

    GetGenerationHelper()->PrefetchSpec(origin.GetOrigin());

    // Processs the password requirements with expected side effects of
    // either storing the requirements from the AutofillQueryResponseContents)
    // in the PasswordRequirementsService.
    GetGenerationHelper()->ProcessPasswordRequirements(forms);

    // Validate the result.
    autofill::FormSignature form_signature =
        autofill::CalculateFormSignature(account_creation_form);
    autofill::FieldSignature field_signature =
        autofill::CalculateFieldSignatureForField(password);
    PasswordRequirementsSpec spec =
        client_->GetPasswordRequirementsService()->GetSpec(
            origin, form_signature, field_signature);
    EXPECT_EQ(test.expected_spec.max_length(), spec.max_length());

    PasswordRequirementsSpec spec_for_unknown_signature =
        client_->GetPasswordRequirementsService()->GetSpec(
            origin, form_signature + 1, field_signature);
    EXPECT_EQ(test.expected_spec_for_unknown_signature.max_length(),
              spec.max_length());
  }
}

TEST_F(PasswordGenerationFrameHelperTest, UpdatePasswordSyncStateIncognito) {
  // Disable password manager by going incognito. Even though password
  // syncing is enabled, generation should still be disabled.
  EXPECT_CALL(*client_, IsSavingAndFillingEnabled(_))
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(*client_, IsIncognito()).WillRepeatedly(testing::Return(true));
  PrefService* prefs = client_->GetPrefs();
  prefs->SetBoolean(prefs::kCredentialsEnableService, true);
  EXPECT_CALL(*client_->GetMockPasswordFeatureManager(), IsGenerationEnabled())
      .WillRepeatedly(testing::Return(true));

  EXPECT_FALSE(IsGenerationEnabled());
}

TEST_F(PasswordGenerationFrameHelperTest, GenerationDisabledForGoogle) {
  EXPECT_CALL(*client_, IsSavingAndFillingEnabled(_))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*client_->GetMockPasswordFeatureManager(), IsGenerationEnabled())
      .WillRepeatedly(testing::Return(true));

  GURL accounts_url = GURL("https://accounts.google.com/path?q=1");
  EXPECT_CALL(*GetTestDriver(), GetLastCommittedURL())
      .WillOnce(testing::ReturnRef(accounts_url));
  EXPECT_FALSE(IsGenerationEnabled());

  GURL myaccount_url = GURL("https://myaccount.google.com/path?q=1");
  EXPECT_CALL(*GetTestDriver(), GetLastCommittedURL())
      .WillOnce(testing::ReturnRef(myaccount_url));
  EXPECT_FALSE(IsGenerationEnabled());

  GURL google_url = GURL("https://subdomain1.subdomain2.google.com/path");
  EXPECT_CALL(*GetTestDriver(), GetLastCommittedURL())
      .WillOnce(testing::ReturnRef(google_url));
  EXPECT_FALSE(IsGenerationEnabled());

  GURL non_google_url = GURL("https://example.com");
  EXPECT_CALL(*GetTestDriver(), GetLastCommittedURL())
      .WillOnce(testing::ReturnRef(non_google_url));
  EXPECT_TRUE(IsGenerationEnabled());
}

}  // namespace password_manager
