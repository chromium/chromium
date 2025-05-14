// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/content_identity_credential_delegate.h"

#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "content/public/browser/federated_auth_autofill_source.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"

namespace autofill {

namespace {

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

class MockFederatedAuthAutofillSource
    : public content::FederatedAuthAutofillSource {
 public:
  MockFederatedAuthAutofillSource() = default;
  ~MockFederatedAuthAutofillSource() override = default;

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

class ContentIdentityCredentialDelegateTest : public ::testing::Test {};

TEST_F(ContentIdentityCredentialDelegateTest, NoPendingRequest) {
  ContentIdentityCredentialDelegate delegate(base::BindLambdaForTesting([]() {
    content::FederatedAuthAutofillSource* result = nullptr;
    return result;
  }));
  std::vector<Suggestion> suggestions =
      delegate.GetVerifiedAutofillSuggestions(EMAIL_ADDRESS);
  ASSERT_EQ(0ul, suggestions.size());
}

TEST_F(ContentIdentityCredentialDelegateTest, NoAccounts) {
  MockFederatedAuthAutofillSource mock;

  ContentIdentityCredentialDelegate delegate(
      base::BindLambdaForTesting([&mock]() {
        content::FederatedAuthAutofillSource* result = &mock;
        return result;
      }));

  EXPECT_CALL(mock, GetAutofillSuggestions).WillOnce(Return(std::nullopt));

  std::vector<Suggestion> suggestions =
      delegate.GetVerifiedAutofillSuggestions(EMAIL_ADDRESS);
  ASSERT_EQ(0ul, suggestions.size());
}

TEST_F(ContentIdentityCredentialDelegateTest, EmptyAccounts) {
  MockFederatedAuthAutofillSource mock;

  ContentIdentityCredentialDelegate delegate(
      base::BindLambdaForTesting([&mock]() {
        content::FederatedAuthAutofillSource* result = &mock;
        return result;
      }));

  std::vector<IdentityRequestAccountPtr> accounts = {};

  EXPECT_CALL(mock, GetAutofillSuggestions).WillOnce(Return(accounts));

  std::vector<Suggestion> suggestions =
      delegate.GetVerifiedAutofillSuggestions(EMAIL_ADDRESS);
  ASSERT_EQ(0ul, suggestions.size());
}

TEST_F(ContentIdentityCredentialDelegateTest, UnsupportedFieldType) {
  MockFederatedAuthAutofillSource mock;

  ContentIdentityCredentialDelegate delegate(
      base::BindLambdaForTesting([&mock]() {
        content::FederatedAuthAutofillSource* result = &mock;
        return result;
      }));

  IdentityRequestAccountPtr account = CreateTestAccount();
  // The delegated flow requires an IdP with a specific format.
  account->identity_provider->format = blink::mojom::Format::kSdJwt;
  account->identity_provider->disclosure_fields = {
      content::IdentityRequestDialogDisclosureField::kEmail};
  std::vector<IdentityRequestAccountPtr> accounts = {account};

  EXPECT_CALL(mock, GetAutofillSuggestions).WillOnce(Return(accounts));

  std::vector<Suggestion> suggestions =
      delegate.GetVerifiedAutofillSuggestions(IBAN_VALUE);
  ASSERT_EQ(0ul, suggestions.size());
}

TEST_F(ContentIdentityCredentialDelegateTest, GetVerifiedEmailRequest) {
  MockFederatedAuthAutofillSource mock;

  ContentIdentityCredentialDelegate delegate(
      base::BindLambdaForTesting([&mock]() {
        content::FederatedAuthAutofillSource* result = &mock;
        return result;
      }));

  IdentityRequestAccountPtr account = CreateTestAccount();
  // The delegated flow requires an IdP with a specific format.
  account->identity_provider->format = blink::mojom::Format::kSdJwt;
  // Use only "email" in the selective disclosure request.
  account->identity_provider->disclosure_fields = {
      content::IdentityRequestDialogDisclosureField::kEmail};
  std::vector<IdentityRequestAccountPtr> accounts = {account};

  EXPECT_CALL(mock, GetAutofillSuggestions).WillOnce(Return(accounts));

  std::vector<Suggestion> suggestions =
      delegate.GetVerifiedAutofillSuggestions(EMAIL_ADDRESS);
  ASSERT_EQ(1ul, suggestions.size());

  Suggestion suggestion = suggestions[0];
  EXPECT_EQ(suggestion.main_text.value, u"john@email.com");
  ASSERT_EQ(suggestion.labels.size(), 1ul);
  ASSERT_EQ(suggestion.minor_texts.size(), 1ul);
  EXPECT_EQ(suggestion.icon, Suggestion::Icon::kEmail);

  // Expect the payload to be populated properly.
  Suggestion::IdentityCredentialPayload payload =
      suggestion.GetPayload<Suggestion::IdentityCredentialPayload>();
  EXPECT_EQ(payload.account_id, "id");
  EXPECT_EQ(payload.config_url, GURL("https://idp.example"));

  // Expect only one field to be available in the payload.
  ASSERT_EQ(payload.fields.size(), 1ul);

  // Expect that email is previewed/filled because it was requested in the
  // conditional request.
  EXPECT_TRUE(payload.fields.contains(EMAIL_ADDRESS));
  EXPECT_EQ(payload.fields[EMAIL_ADDRESS], u"john@email.com");

  // Expect that name isn't previewed/filled because it wasn't requested in the
  // conditional request.
  EXPECT_FALSE(payload.fields.contains(NAME_FULL));
}

TEST_F(ContentIdentityCredentialDelegateTest, SuggestPhoneNumbers) {
  MockFederatedAuthAutofillSource mock;

  ContentIdentityCredentialDelegate delegate(
      base::BindLambdaForTesting([&mock]() {
        content::FederatedAuthAutofillSource* result = &mock;
        return result;
      }));

  IdentityRequestAccountPtr account = CreateTestAccount();
  // The delegated flow requires an IdP with a specific format.
  account->identity_provider->format = blink::mojom::Format::kSdJwt;
  // Use "email" AND "phone-number" in the selective disclosure request.
  account->identity_provider->disclosure_fields = {
      content::IdentityRequestDialogDisclosureField::kPhoneNumber,
      content::IdentityRequestDialogDisclosureField::kEmail};
  std::vector<IdentityRequestAccountPtr> accounts = {account};

  EXPECT_CALL(mock, GetAutofillSuggestions).WillOnce(Return(accounts));

  std::vector<Suggestion> suggestions =
      delegate.GetVerifiedAutofillSuggestions(PHONE_HOME_WHOLE_NUMBER);
  ASSERT_EQ(1ul, suggestions.size());

  Suggestion suggestion = suggestions[0];
  EXPECT_EQ(suggestion.main_text.value, u"+1 (234) 567-8910");
  ASSERT_EQ(suggestion.minor_texts.size(), 1ul);

  // Expect the payload to be populated properly.
  Suggestion::IdentityCredentialPayload payload =
      suggestion.GetPayload<Suggestion::IdentityCredentialPayload>();
  EXPECT_EQ(payload.account_id, "id");
  EXPECT_EQ(payload.config_url, GURL("https://idp.example"));

  // Expect two fields to be available in the payload: emails and usernames.
  ASSERT_EQ(payload.fields.size(), 2ul);

  // Expect that email is previewed/filled because it was requested in the
  // conditional request.
  EXPECT_TRUE(payload.fields.contains(EMAIL_ADDRESS));
  EXPECT_EQ(payload.fields[EMAIL_ADDRESS], u"john@email.com");

  // Expect that email is previewed/filled because it was requested in the
  // conditional request.
  EXPECT_TRUE(payload.fields.contains(PHONE_HOME_WHOLE_NUMBER));
  EXPECT_EQ(payload.fields[PHONE_HOME_WHOLE_NUMBER], u"+1 (234) 567-8910");
}

TEST_F(ContentIdentityCredentialDelegateTest,
       GetSuggestionForFieldThatWasntRequested) {
  MockFederatedAuthAutofillSource mock;

  ContentIdentityCredentialDelegate delegate(
      base::BindLambdaForTesting([&mock]() {
        content::FederatedAuthAutofillSource* result = &mock;
        return result;
      }));

  IdentityRequestAccountPtr account = CreateTestAccount();
  // The delegated flow requires an IdP with a specific format.
  account->identity_provider->format = blink::mojom::Format::kSdJwt;
  // Use only "email" in the selective disclosure request.
  account->identity_provider->disclosure_fields = {
      content::IdentityRequestDialogDisclosureField::kEmail};
  std::vector<IdentityRequestAccountPtr> accounts = {account};

  EXPECT_CALL(mock, GetAutofillSuggestions).WillOnce(Return(accounts));

  std::vector<Suggestion> suggestions =
      delegate.GetVerifiedAutofillSuggestions(NAME_FULL);
  ASSERT_EQ(0ul, suggestions.size());
}

TEST_F(ContentIdentityCredentialDelegateTest,
       GetSuggestionForFieldThatRequestedButIsUnavailable) {
  MockFederatedAuthAutofillSource mock;

  ContentIdentityCredentialDelegate delegate(
      base::BindLambdaForTesting([&mock]() {
        content::FederatedAuthAutofillSource* result = &mock;
        return result;
      }));

  IdentityRequestAccountPtr account = CreateTestAccount();

  // The delegated flow requires an IdP with a specific format.
  account->identity_provider->format = blink::mojom::Format::kSdJwt;

  // Set email to an unavailable string.
  account->email = "";

  // Use only "email" in the selective disclosure request.
  account->identity_provider->disclosure_fields = {
      content::IdentityRequestDialogDisclosureField::kEmail};
  std::vector<IdentityRequestAccountPtr> accounts = {account};

  EXPECT_CALL(mock, GetAutofillSuggestions).WillOnce(Return(accounts));

  std::vector<Suggestion> suggestions =
      delegate.GetVerifiedAutofillSuggestions(EMAIL_ADDRESS);
  ASSERT_EQ(0ul, suggestions.size());
}

TEST_F(ContentIdentityCredentialDelegateTest,
       GetSuggestionsForDelegatedCredentialAvailableForSignUp) {
  MockFederatedAuthAutofillSource mock;

  ContentIdentityCredentialDelegate delegate(
      base::BindLambdaForTesting([&mock]() {
        content::FederatedAuthAutofillSource* result = &mock;
        return result;
      }));

  IdentityRequestAccountPtr account = CreateTestAccount();
  // The delegated flow requires an IdP with a specific format.
  account->identity_provider->format = blink::mojom::Format::kSdJwt;
  account->login_state = content::IdentityRequestAccount::LoginState::kSignUp;
  std::vector<IdentityRequestAccountPtr> accounts = {account};

  EXPECT_CALL(mock, GetAutofillSuggestions).WillOnce(Return(accounts));

  std::vector<Suggestion> suggestions =
      delegate.GetVerifiedAutofillSuggestions(PASSWORD);
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

TEST_F(ContentIdentityCredentialDelegateTest,
       GetSuggestionsForPasswordUnavailableForSignUp) {
  MockFederatedAuthAutofillSource mock;

  ContentIdentityCredentialDelegate delegate(
      base::BindLambdaForTesting([&mock]() {
        content::FederatedAuthAutofillSource* result = &mock;
        return result;
      }));

  IdentityRequestAccountPtr account = CreateTestAccount();
  account->login_state = content::IdentityRequestAccount::LoginState::kSignUp;
  std::vector<IdentityRequestAccountPtr> accounts = {account};

  EXPECT_CALL(mock, GetAutofillSuggestions).WillOnce(Return(accounts));

  std::vector<Suggestion> suggestions =
      delegate.GetVerifiedAutofillSuggestions(PASSWORD);
  EXPECT_TRUE(suggestions.empty());
}

TEST_F(ContentIdentityCredentialDelegateTest,
       GetSuggestionsForPasswordAvailableForSignIn) {
  MockFederatedAuthAutofillSource mock;

  ContentIdentityCredentialDelegate delegate(
      base::BindLambdaForTesting([&mock]() {
        content::FederatedAuthAutofillSource* result = &mock;
        return result;
      }));

  IdentityRequestAccountPtr account = CreateTestAccount();
  account->login_state = content::IdentityRequestAccount::LoginState::kSignIn;
  std::vector<IdentityRequestAccountPtr> accounts = {account};

  EXPECT_CALL(mock, GetAutofillSuggestions).WillOnce(Return(accounts));

  std::vector<Suggestion> suggestions =
      delegate.GetVerifiedAutofillSuggestions(PASSWORD);
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

TEST_F(ContentIdentityCredentialDelegateTest, GetProvidedNameRequest) {
  MockFederatedAuthAutofillSource mock;

  ContentIdentityCredentialDelegate delegate(
      base::BindLambdaForTesting([&mock]() {
        content::FederatedAuthAutofillSource* result = &mock;
        return result;
      }));

  IdentityRequestAccountPtr account = CreateTestAccount();
  // The delegated flow requires an IdP with a specific format.
  account->identity_provider->format = blink::mojom::Format::kSdJwt;
  // Use only "name" in the selective disclosure request.
  account->identity_provider->disclosure_fields = {
      content::IdentityRequestDialogDisclosureField::kName};
  std::vector<IdentityRequestAccountPtr> accounts = {account};

  EXPECT_CALL(mock, GetAutofillSuggestions).WillOnce(Return(accounts));

  std::vector<Suggestion> suggestions =
      delegate.GetVerifiedAutofillSuggestions(NAME_FULL);
  ASSERT_EQ(1ul, suggestions.size());

  Suggestion suggestion = suggestions[0];
  EXPECT_EQ(suggestion.main_text.value, u"John");
  EXPECT_EQ(suggestion.labels.size(), 0ul);
  EXPECT_EQ(suggestion.minor_texts.size(), 1ul);
  EXPECT_EQ(suggestion.icon, Suggestion::Icon::kAccount);

  // Expect the payload to be populated properly.
  Suggestion::IdentityCredentialPayload payload =
      suggestion.GetPayload<Suggestion::IdentityCredentialPayload>();
  EXPECT_EQ(payload.account_id, "id");
  EXPECT_EQ(payload.config_url, GURL("https://idp.example"));

  // Expect only one field to be available in the payload.
  ASSERT_EQ(payload.fields.size(), 1ul);

  // Expect that email is previewed/filled because it was requested in the
  // conditional request.
  EXPECT_TRUE(payload.fields.contains(NAME_FULL));
  EXPECT_EQ(payload.fields[NAME_FULL], u"John");
}

}  // namespace

}  // namespace autofill
