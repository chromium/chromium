// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/content_identity_credential_delegate.h"

#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/common/autofill_test_util.h"
#include "content/public/browser/webid/autofill_source.h"
#include "content/public/browser/webid/identity_request_dialog_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/webid/federated_request.mojom.h"

namespace autofill {

namespace {

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

class MockAutofillSource : public content::webid::AutofillSource {
 public:
  MockAutofillSource() = default;
  ~MockAutofillSource() override = default;

  MOCK_METHOD(const std::optional<std::vector<IdentityRequestAccountPtr>>,
              GetAutofillSuggestions,
              (),
              (const override));
  MOCK_METHOD(void,
              NotifyAutofillSuggestionAccepted,
              (const GURL& idp,
               const std::string& account_id,
               bool show_modal,
               OnFederatedTokenReceivedCallback callback),
              (override));
};

// Creates a generic test account with a mediated IdP by default. To test
// delegated IdP, the caller should update the identity provider's format to
// kSdJwt.
IdentityRequestAccountPtr CreateTestAccount() {
  IdentityRequestAccountPtr account =
      base::MakeRefCounted<content::IdentityRequestAccount>(
          "id", "display_identifier", "display_name", "john@email.com", "John",
          "given_name", GURL(), "+1 (234) 567-8910", "username",
          /*potentially_approved_origin_hashes=*/std::vector<std::string>(),
          /*login_hints=*/std::vector<std::string>(),
          /*domain_hints=*/std::vector<std::string>(),
          /*labels=*/std::vector<std::string>());

  content::IdentityProviderMetadata metadata;
  metadata.config_url = GURL("https://idp.example");
  content::ClientMetadata client((GURL()), (GURL()), (GURL()), (gfx::Image()));
  std::vector<content::IdentityRequestDialogDisclosureField> disclosures;

  scoped_refptr<content::IdentityProviderData> identity_provider_data =
      base::MakeRefCounted<content::IdentityProviderData>(
          "idp.example", metadata, client, blink::mojom::RpContext::kSignIn,
          /*format=*/std::nullopt, disclosures, false);

  account->identity_provider = identity_provider_data;

  return account;
}

class ContentIdentityCredentialDelegateTest : public ::testing::Test {
 public:
  ContentIdentityCredentialDelegateTest() {
    FormData form;
    form.set_url(GURL("https://www.foo.com"));
    form.set_fields({test::CreateTestFormField(
        "unclassifiable label", "unclassifiable name", "unclassifiable value",
        FormControlType::kInputText)});
    form_structure_ = std::make_unique<FormStructure>(form);
  }

  TestAutofillClient& client() { return autofill_client_; }
  FormStructure& form() { return *form_structure_; }
  AutofillField& field() { return *form_structure_->fields().front(); }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  TestAutofillClient autofill_client_;
  std::unique_ptr<FormStructure> form_structure_;
};

TEST_F(ContentIdentityCredentialDelegateTest, NoPendingRequest) {
  ContentIdentityCredentialDelegate delegate(base::BindLambdaForTesting([]() {
    content::webid::AutofillSource* result = nullptr;
    return result;
  }));
  test_api(form()).SetFieldTypes({EMAIL_ADDRESS});
  std::vector<Suggestion> suggestions = delegate.GetVerifiedAutofillSuggestions(
      form().ToFormData(), &form(), field(), &field(), client());
  ASSERT_EQ(0ul, suggestions.size());
}

TEST_F(ContentIdentityCredentialDelegateTest, NoAccounts) {
  MockAutofillSource mock;

  ContentIdentityCredentialDelegate delegate(
      base::BindLambdaForTesting([&mock]() {
        content::webid::AutofillSource* result = &mock;
        return result;
      }));

  EXPECT_CALL(mock, GetAutofillSuggestions).WillOnce(Return(std::nullopt));

  test_api(form()).SetFieldTypes({PASSWORD});
  std::vector<Suggestion> suggestions = delegate.GetVerifiedAutofillSuggestions(
      form().ToFormData(), &form(), field(), &field(), client());
  ASSERT_EQ(0ul, suggestions.size());
}

TEST_F(ContentIdentityCredentialDelegateTest, EmptyAccounts) {
  MockAutofillSource mock;

  ContentIdentityCredentialDelegate delegate(
      base::BindLambdaForTesting([&mock]() {
        content::webid::AutofillSource* result = &mock;
        return result;
      }));

  std::vector<IdentityRequestAccountPtr> accounts = {};

  EXPECT_CALL(mock, GetAutofillSuggestions).WillOnce(Return(accounts));

  test_api(form()).SetFieldTypes({PASSWORD});
  std::vector<Suggestion> suggestions = delegate.GetVerifiedAutofillSuggestions(
      form().ToFormData(), &form(), field(), &field(), client());
  ASSERT_EQ(0ul, suggestions.size());
}

TEST_F(ContentIdentityCredentialDelegateTest,
       NoSuggestionsForUnsupportedFields) {
  MockAutofillSource mock;

  ContentIdentityCredentialDelegate delegate(
      base::BindLambdaForTesting([&mock]() {
        content::webid::AutofillSource* result = &mock;
        return result;
      }));

  EXPECT_CALL(mock, GetAutofillSuggestions).Times(0);

  for (FieldType unsupported_type :
       {UNKNOWN_TYPE, EMAIL_ADDRESS, PHONE_HOME_WHOLE_NUMBER, NAME_FULL,
        NAME_FIRST, USERNAME_AND_EMAIL_ADDRESS, CREDIT_CARD_NUMBER}) {
    SCOPED_TRACE(testing::Message() << "Testing FieldType: "
                                    << FieldTypeToStringView(unsupported_type));
    test_api(form()).SetFieldTypes({unsupported_type});
    std::vector<Suggestion> suggestions =
        delegate.GetVerifiedAutofillSuggestions(form().ToFormData(), &form(),
                                                field(), &field(), client());
    EXPECT_TRUE(suggestions.empty());
  }
}

TEST_F(ContentIdentityCredentialDelegateTest,
       GetSuggestionsForPasswordUnavailableForSignUp) {
  MockAutofillSource mock;

  ContentIdentityCredentialDelegate delegate(
      base::BindLambdaForTesting([&mock]() {
        content::webid::AutofillSource* result = &mock;
        return result;
      }));

  IdentityRequestAccountPtr account = CreateTestAccount();
  account->idp_claimed_login_state =
      content::IdentityRequestAccount::LoginState::kSignUp;
  std::vector<IdentityRequestAccountPtr> accounts = {account};

  EXPECT_CALL(mock, GetAutofillSuggestions).WillOnce(Return(accounts));

  test_api(form()).SetFieldTypes({PASSWORD});
  std::vector<Suggestion> suggestions = delegate.GetVerifiedAutofillSuggestions(
      form().ToFormData(), &form(), field(), &field(), client());
  EXPECT_TRUE(suggestions.empty());
}

TEST_F(ContentIdentityCredentialDelegateTest,
       GetSuggestionsForPasswordAvailableForSignIn) {
  MockAutofillSource mock;

  ContentIdentityCredentialDelegate delegate(
      base::BindLambdaForTesting([&mock]() {
        content::webid::AutofillSource* result = &mock;
        return result;
      }));

  IdentityRequestAccountPtr account = CreateTestAccount();
  account->idp_claimed_login_state =
      content::IdentityRequestAccount::LoginState::kSignIn;
  std::vector<IdentityRequestAccountPtr> accounts = {account};

  EXPECT_CALL(mock, GetAutofillSuggestions).WillOnce(Return(accounts));

  test_api(form()).SetFieldTypes({PASSWORD});
  std::vector<Suggestion> suggestions = delegate.GetVerifiedAutofillSuggestions(
      form().ToFormData(), &form(), field(), &field(), client());
  ASSERT_EQ(1ul, suggestions.size());

  Suggestion suggestion = suggestions[0];
  EXPECT_EQ(suggestion.main_text.value, u"john@email.com");
  EXPECT_EQ(suggestion.labels.size(), 1ul);
  EXPECT_EQ(suggestion.minor_texts.size(), 0ul);

  // Expect the payload to be populated properly.
  Suggestion::IdentityCredentialPayload payload =
      suggestion.GetPayload<Suggestion::IdentityCredentialPayload>();
  EXPECT_EQ(payload.account_id, "id");
  EXPECT_EQ(payload.config_url, GURL("https://idp.example"));

  // Expect no field to be available in the payload for PASSWORD.
  EXPECT_TRUE(payload.fields.empty());
}

}  // namespace

}  // namespace autofill
