// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/suggestions/identity_credential_suggestion_generator.h"

#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/data_model/identity_credential/identity_credential.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "content/public/browser/webid/autofill_source.h"
#include "content/public/browser/webid/identity_request_dialog_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"

namespace autofill {
namespace {

using testing::_;
using testing::Return;

class MockFederatedAuthAutofillSource : public content::webid::AutofillSource {
 public:
  MockFederatedAuthAutofillSource() = default;
  ~MockFederatedAuthAutofillSource() override = default;

  MOCK_METHOD(const std::optional<std::vector<IdentityRequestAccountPtr>>,
              GetAutofillSuggestions,
              (),
              (const, override));
  MOCK_METHOD(
      void,
      NotifyAutofillSuggestionAccepted,
      (const GURL& idp,
       const std::string& account_id,
       bool show_modal,
       IdentityCredentialDelegate::OnFederatedTokenReceivedCallback callback),
      (override));
};

class IdentityCredentialSuggestionGeneratorTest : public testing::Test {
 protected:
  IdentityCredentialSuggestionGeneratorTest() {
    FormData form_data;
    form_data.set_fields({test::CreateTestFormField(
        "Email", "email", "", FormControlType::kInputEmail)});
    form_data.set_main_frame_origin(
        url::Origin::Create(GURL("https://www.example.com")));
    form_structure_ = std::make_unique<FormStructure>(form_data);
    autofill_field_ = form_structure_->field(0);
    test_api(*form_structure_).SetFieldTypes({EMAIL_ADDRESS});
  }

  TestAutofillClient& client() { return autofill_client_; }
  AutofillField& field() { return *autofill_field_; }
  FormStructure& form() { return *form_structure_; }

  scoped_refptr<content::IdentityRequestAccount> CreateTestAccount() {
    scoped_refptr<content::IdentityRequestAccount> account =
        base::MakeRefCounted<content::IdentityRequestAccount>(
            "id", "display_identifier", "display_name", "john@email.com",
            "John", "given_name", GURL(), "+1 (234) 567-8910", "username",
            /*login_hints=*/std::vector<std::string>(),
            /*domain_hints=*/std::vector<std::string>(),
            /*labels=*/std::vector<std::string>());

    content::IdentityProviderMetadata metadata;
    metadata.config_url = GURL("https://idp.example");
    account->identity_provider =
        base::MakeRefCounted<content::IdentityProviderData>(
            "idp.example", metadata,
            content::ClientMetadata(GURL(), GURL(), GURL(), gfx::Image()),
            blink::mojom::RpContext::kSignIn,
            /*format=*/std::nullopt,
            std::vector<content::IdentityRequestDialogDisclosureField>(),
            /*is_auto_reauthn=*/false);
    account->identity_provider->format = blink::mojom::Format::kSdJwt;
    account->idp_claimed_login_state =
        content::IdentityRequestAccount::LoginState::kSignIn;
    account->identity_provider->disclosure_fields = {
        content::IdentityRequestDialogDisclosureField::kEmail};
    return account;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  TestAutofillClient autofill_client_;
  std::unique_ptr<FormStructure> form_structure_;
  // Owned by `form_structure_`.
  raw_ptr<AutofillField> autofill_field_ = nullptr;
};

// Checks that identity credential suggestion is generated.
TEST_F(IdentityCredentialSuggestionGeneratorTest, GeneratesSuggestion) {
  base::MockCallback<base::OnceCallback<void(
      std::pair<SuggestionGenerator::SuggestionDataSource,
                std::vector<SuggestionGenerator::SuggestionData>>)>>
      suggestion_data_callback;
  base::MockCallback<
      base::OnceCallback<void(SuggestionGenerator::ReturnedSuggestions)>>
      suggestions_generated_callback;

  MockFederatedAuthAutofillSource mock;

  IdentityCredentialSuggestionGenerator generator(base::BindLambdaForTesting(
      [&mock]() -> content::webid::AutofillSource* { return &mock; }));
  scoped_refptr<content::IdentityRequestAccount> account = CreateTestAccount();

  EXPECT_CALL(mock, GetAutofillSuggestions)
      .WillOnce(
          Return(std::vector<scoped_refptr<content::IdentityRequestAccount>>{
              account}));

  std::pair<SuggestionGenerator::SuggestionDataSource,
            std::vector<SuggestionGenerator::SuggestionData>>
      saved_suggestion_data;
  EXPECT_CALL(
      suggestion_data_callback,
      Run(testing::Pair(
          SuggestionGenerator::SuggestionDataSource::kIdentityCredential,
          testing::SizeIs(1))))
      .WillOnce(testing::SaveArg<0>(&saved_suggestion_data));
  generator.FetchSuggestionData(form().ToFormData(), field(), &form(), &field(),
                                client(), suggestion_data_callback.Get());

  SuggestionGenerator::ReturnedSuggestions generated_suggestions;
  EXPECT_CALL(suggestions_generated_callback,
              Run(testing::Pair(FillingProduct::kIdentityCredential,
                                testing::SizeIs(1))))
      .WillOnce(testing::SaveArg<0>(&generated_suggestions));
  generator.GenerateSuggestions(form().ToFormData(), field(), &form(), &field(),
                                client(), {saved_suggestion_data},
                                suggestions_generated_callback.Get());

  const Suggestion& suggestion = generated_suggestions.second[0];
  EXPECT_EQ(suggestion.main_text.value, u"john@email.com");
  ASSERT_EQ(suggestion.labels.size(), 1ul);
  ASSERT_EQ(suggestion.minor_texts.size(), 1ul);
  EXPECT_EQ(suggestion.icon, Suggestion::Icon::kEmail);

  // Expect the payload to be populated properly.
  const Suggestion::IdentityCredentialPayload& payload =
      suggestion.GetPayload<Suggestion::IdentityCredentialPayload>();
  EXPECT_EQ(payload.account_id, "id");
  EXPECT_EQ(payload.config_url, GURL("https://idp.example"));
}

}  // namespace
}  // namespace autofill
