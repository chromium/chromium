// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_generation_frame_helper.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/containers/flat_map.h"
#include "base/metrics/field_trial.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/log_event.h"
#include "components/autofill/core/browser/proto/password_requirements.pb.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/password_form_generation_data.h"
#include "components/autofill/core/common/signatures.h"
#include "components/password_manager/core/browser/generation/password_generator.h"
#include "components/password_manager/core/browser/generation/password_requirements_spec_fetcher.h"
#include "components/password_manager/core/browser/password_autofill_manager.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_requirements_service.h"
#include "components/password_manager/core/browser/password_store/mock_password_store_interface.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using autofill::AutofillField;
using autofill::AutofillType;
using autofill::FieldGlobalId;
using autofill::FieldSignature;
using autofill::FieldType;
using autofill::FormData;
using autofill::FormSignature;
using autofill::FormStructure;
using autofill::PasswordRequirementsSpec;
using autofill::password_generation::PasswordGenerationType;
using autofill::test::CreateFieldPrediction;
using autofill::test::CreateTestFormField;
using base::ASCIIToUTF16;
using ::testing::_;
using ::testing::NiceMock;

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

  // PasswordManagerDriver implementation.
  PasswordGenerationFrameHelper* GetPasswordGenerationHelper() override {
    return &password_generation_manager_;
  }
  PasswordManager* GetPasswordManager() override { return &password_manager_; }
  PasswordAutofillManager* GetPasswordAutofillManager() override {
    return &password_autofill_manager_;
  }

  MOCK_METHOD(GURL&, GetLastCommittedURL, (), (const override));

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
    if (origin.DeprecatedGetOriginAsURL().host_piece().find(
            kNoServerResponse) != std::string::npos) {
      std::move(callback).Run(PasswordRequirementsSpec());
    } else if (origin.DeprecatedGetOriginAsURL().host_piece().find(
                   kHasServerResponse) != std::string::npos) {
      std::move(callback).Run(GetDomainWideRequirements());
    } else {
      NOTREACHED_IN_MIGRATION();
    }
  }
};

class MockPasswordManagerClient : public StubPasswordManagerClient {
 public:
  MOCK_METHOD(bool, IsSavingAndFillingEnabled, (const GURL&), (const override));
  MOCK_METHOD(bool, IsOffTheRecord, (), (const override));

  explicit MockPasswordManagerClient(std::unique_ptr<PrefService> prefs)
      : prefs_(std::move(prefs)),
        store_(new MockPasswordStoreInterface),
        driver_(this),
        password_requirements_service_(
            std::make_unique<FakePasswordRequirementsSpecFetcher>()) {}

  ~MockPasswordManagerClient() override = default;

  PasswordStoreInterface* GetProfilePasswordStore() const override {
    return store_.get();
  }
  PrefService* GetPrefs() const override { return prefs_.get(); }
  PasswordRequirementsService* GetPasswordRequirementsService() override {
    return &password_requirements_service_;
  }
  void SetLastCommittedEntryUrl(const GURL& url) {
    last_committed_origin_ = url::Origin::Create(url);
  }
  url::Origin GetLastCommittedOrigin() const override {
    return last_committed_origin_;
  }

  TestPasswordManagerDriver* test_driver() { return &driver_; }
  MockPasswordStoreInterface* mock_store() { return store_.get(); }

 private:
  autofill::test::AutofillUnitTestEnvironment autofill_environment_;
  std::unique_ptr<PrefService> prefs_;
  scoped_refptr<MockPasswordStoreInterface> store_;
  NiceMock<TestPasswordManagerDriver> driver_;
  PasswordRequirementsService password_requirements_service_;
  url::Origin last_committed_origin_;
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
#if BUILDFLAG(IS_ANDROID)
    prefs->registry()->RegisterIntegerPref(
        password_manager::prefs::kPasswordsUseUPMLocalAndSeparateStores, 0);
#endif
    client_ = std::make_unique<MockPasswordManagerClient>(std::move(prefs));
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
  // If password store is not able to save passwords generation is disabled.
  EXPECT_CALL(*client_->mock_store(), IsAbleToSavePasswords())
      .WillOnce(testing::Return(false));
  EXPECT_FALSE(IsGenerationEnabled());

  // Enabling the PasswordManager and password sync should cause generation to
  // be enabled, unless the sync is with a custom passphrase.
  EXPECT_CALL(*client_->mock_store(), IsAbleToSavePasswords())
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*client_, IsSavingAndFillingEnabled(_))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*client_->GetPasswordFeatureManager(), IsGenerationEnabled())
      .WillRepeatedly(testing::Return(true));
  EXPECT_TRUE(IsGenerationEnabled());

  // Disabling password syncing should cause generation to be disabled.
  EXPECT_CALL(*client_->mock_store(), IsAbleToSavePasswords())
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*client_->GetPasswordFeatureManager(), IsGenerationEnabled())
      .WillRepeatedly(testing::Return(false));
  EXPECT_FALSE(IsGenerationEnabled());

  // Disabling the PasswordManager should cause generation to be disabled even
  // if syncing is enabled.
  EXPECT_CALL(*client_->mock_store(), IsAbleToSavePasswords())
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*client_, IsSavingAndFillingEnabled(_))
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(*client_->GetPasswordFeatureManager(), IsGenerationEnabled())
      .WillRepeatedly(testing::Return(true));
  EXPECT_FALSE(IsGenerationEnabled());
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(PasswordGenerationFrameHelperTest,
       GenerationDisabledDueToOutdatedGMSCore) {
  EXPECT_CALL(*client_->mock_store(), IsAbleToSavePasswords())
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*client_, IsSavingAndFillingEnabled(_))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*client_->GetPasswordFeatureManager(), ShouldUpdateGmsCore())
      .WillRepeatedly(testing::Return(false));
  EXPECT_FALSE(IsGenerationEnabled());
}
#endif

// Verify that password requirements received from the autofill server are
// stored and that domain-wide password requirements are fetched as well.
TEST_F(PasswordGenerationFrameHelperTest, ProcessPasswordRequirements) {
  EXPECT_CALL(*client_->mock_store(), IsAbleToSavePasswords())
      .WillRepeatedly(testing::Return(true));

  // Setup so that IsGenerationEnabled() returns true.
  EXPECT_CALL(*client_, IsSavingAndFillingEnabled(_))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*client_->GetPasswordFeatureManager(), IsGenerationEnabled())
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

    autofill::FormFieldData username =
        CreateTestFormField(/*label=*/"", /*name=*/"login", /*value=*/"",
                            autofill::FormControlType::kInputText);
    autofill::FormFieldData password = CreateTestFormField(
        /*label=*/"", /*name=*/base::StringPrintf("password%d", test_counter),
        /*value=*/"", autofill::FormControlType::kInputPassword);

    // Configure the last committed entry URL with some magic constants for
    // which the FakePasswordRequirementsFetcher is configured to respond
    // with a filled or empty response.
    GURL origin(base::StringPrintf("https://%d-%s/", test_counter,
                                   test.has_domain_wide_requirements
                                       ? kHasServerResponse
                                       : kNoServerResponse));

    FormData account_creation_form;
    account_creation_form.set_url(origin);
    account_creation_form.set_action(origin);
    account_creation_form.set_name(u"account_creation_form");
    account_creation_form.set_fields({username, password});

    client_->SetLastCommittedEntryUrl(origin);
    GetGenerationHelper()->PrefetchSpec(origin.DeprecatedGetOriginAsURL());

    // Processs the password requirements with expected side effects of
    // either storing the requirements from the AutofillQueryResponseContents)
    // in the PasswordRequirementsService.
    base::flat_map<FieldGlobalId, AutofillType::ServerPrediction> predictions;

    AutofillType::ServerPrediction username_prediction;
    username_prediction.server_predictions = {
        autofill::test::CreateFieldPrediction(FieldType::EMAIL_ADDRESS,
                                              /*is_override=*/false)};
    AutofillType::ServerPrediction password_prediction;
    password_prediction.server_predictions = {
        autofill::test::CreateFieldPrediction(
            FieldType::ACCOUNT_CREATION_PASSWORD,
            /*is_override=*/false)};
    if (test.has_field_requirements) {
      password_prediction.password_requirements = GetFieldRequirements();
    }

    predictions.insert({username.global_id(), std::move(username_prediction)});
    predictions.insert({password.global_id(), std::move(password_prediction)});

    GetGenerationHelper()->ProcessPasswordRequirements(account_creation_form,
                                                       predictions);

    // Validate the result.
    FormSignature form_signature =
        autofill::CalculateFormSignature(account_creation_form);
    autofill::FieldSignature field_signature =
        autofill::CalculateFieldSignatureForField(password);
    PasswordRequirementsSpec spec =
        client_->GetPasswordRequirementsService()->GetSpec(
            origin, form_signature, field_signature);
    EXPECT_EQ(test.expected_spec.max_length(), spec.max_length());

    PasswordRequirementsSpec spec_for_unknown_signature =
        client_->GetPasswordRequirementsService()->GetSpec(
            origin, FormSignature(form_signature.value() + 1), field_signature);
    EXPECT_EQ(test.expected_spec_for_unknown_signature.max_length(),
              spec.max_length());
  }
}

TEST_F(PasswordGenerationFrameHelperTest, UpdatePasswordSyncStateIncognito) {
  // Disable password manager by going incognito. Even though password
  // syncing is enabled, generation should still be disabled.
  EXPECT_CALL(*client_, IsSavingAndFillingEnabled(_))
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(*client_, IsOffTheRecord()).WillRepeatedly(testing::Return(true));
  PrefService* prefs = client_->GetPrefs();
  prefs->SetBoolean(prefs::kCredentialsEnableService, true);
  EXPECT_CALL(*client_->GetPasswordFeatureManager(), IsGenerationEnabled())
      .WillRepeatedly(testing::Return(true));

  EXPECT_FALSE(IsGenerationEnabled());
}

TEST_F(PasswordGenerationFrameHelperTest, GenerationDisabledForGoogle) {
  EXPECT_CALL(*client_->mock_store(), IsAbleToSavePasswords())
      .WillRepeatedly(testing::Return(true));

  EXPECT_CALL(*client_, IsSavingAndFillingEnabled(_))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*client_->GetPasswordFeatureManager(), IsGenerationEnabled())
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

TEST_F(PasswordGenerationFrameHelperTest, ShortCrowdsourcedPasswordLength) {
  const GURL kTestOrigin("https://example.com");
  constexpr FormSignature kTestFormSignature(123);
  constexpr FieldSignature kTestFieldSignature(456);

  // Create a spec with a length shorter than default.
  PasswordRequirementsSpec spec;
  constexpr size_t kShortLength = 9u;
  spec.set_max_length(kShortLength);
  spec.set_priority(10);
  client_->GetPasswordRequirementsService()->AddSpec(
      kTestOrigin, kTestFormSignature, kTestFieldSignature, spec);

  // Check that crowdsourced length is used when password is generated
  // automatically.
  std::u16string generated_pwd = GetGenerationHelper()->GeneratePassword(
      kTestOrigin, PasswordGenerationType::kAutomatic, kTestFormSignature,
      kTestFieldSignature,
      /*max_length=*/0);
  EXPECT_EQ(generated_pwd.size(), kShortLength);

  // Check that crowdsourced length is ignored when password generation is
  // triggered on the manual fallback.
  generated_pwd = GetGenerationHelper()->GeneratePassword(
      kTestOrigin, PasswordGenerationType::kManual, kTestFormSignature,
      kTestFieldSignature,
      /*max_length=*/0);
  EXPECT_EQ(generated_pwd.size(), autofill::kDefaultPasswordLength);
}

}  // namespace password_manager
