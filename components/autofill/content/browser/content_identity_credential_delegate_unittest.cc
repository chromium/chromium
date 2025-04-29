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

class ContentIdentityCredentialDelegateTest : public ::testing::Test {};

TEST_F(ContentIdentityCredentialDelegateTest, NoPendingRequest) {
  ContentIdentityCredentialDelegate delegate(base::BindLambdaForTesting([]() {
    content::FederatedAuthAutofillSource* result = nullptr;
    return result;
  }));
  std::vector<Suggestion> suggestions =
      delegate.GetVerifiedAutofillSuggestions(EMAIL_ADDRESS);
  EXPECT_EQ(0ul, suggestions.size());
}

TEST_F(ContentIdentityCredentialDelegateTest, GetVerifiedEmailRequest) {
  MockFederatedAuthAutofillSource mock;

  ContentIdentityCredentialDelegate delegate(
      base::BindLambdaForTesting([&mock]() {
        content::FederatedAuthAutofillSource* result = &mock;
        return result;
      }));

  IdentityRequestAccountPtr account =
      base::MakeRefCounted<content::IdentityRequestAccount>(
          "id", "display_identifier", "display_name", "john@email.com", "name",
          "given_name", GURL(), "phone", "username",
          /*login_hints=*/std::vector<std::string>(),
          /*domain_hints=*/std::vector<std::string>(),
          /*labels=*/std::vector<std::string>());
  content::IdentityProviderMetadata metadata;
  metadata.config_url = GURL("https://idp.example");

  std::vector<content::IdentityRequestDialogDisclosureField> disclosures;

  scoped_refptr<content::IdentityProviderData> identity_provider =
      base::MakeRefCounted<content::IdentityProviderData>(
          "idp.example", metadata,
          content::ClientMetadata((GURL()), (GURL()), (GURL()), (gfx::Image())),
          blink::mojom::RpContext::kSignIn, disclosures, false);

  account->identity_provider = identity_provider;
  std::vector<IdentityRequestAccountPtr> accounts = {account};

  EXPECT_CALL(mock, GetAutofillSuggestions).WillOnce(Return(accounts));

  std::vector<Suggestion> suggestions =
      delegate.GetVerifiedAutofillSuggestions(EMAIL_ADDRESS);
  EXPECT_EQ(1ul, suggestions.size());

  Suggestion suggestion = suggestions[0];
  EXPECT_EQ(suggestion.main_text.value, u"john@email.com");
  EXPECT_EQ(suggestion.labels.size(), 1ul);
  EXPECT_EQ(suggestion.minor_texts.size(), 1ul);

  // Expect the payload to be populated properly.
  Suggestion::IdentityCredentialPayload payload =
      suggestion.GetPayload<Suggestion::IdentityCredentialPayload>();
  EXPECT_EQ(payload.account_id, "id");
  EXPECT_EQ(payload.config_url, GURL("https://idp.example"));
  EXPECT_EQ(payload.fields.size(), 3ul);
  EXPECT_TRUE(payload.fields.contains(EMAIL_ADDRESS));
  EXPECT_EQ(payload.fields[EMAIL_ADDRESS], u"john@email.com");
  EXPECT_TRUE(payload.fields.contains(NAME_FULL));
  EXPECT_EQ(payload.fields[NAME_FULL], u"name");
  EXPECT_TRUE(payload.fields.contains(NAME_FIRST));
  EXPECT_EQ(payload.fields[NAME_FIRST], u"given_name");
}

TEST_F(ContentIdentityCredentialDelegateTest, GetSuggestionsForPassword) {
  MockFederatedAuthAutofillSource mock;

  ContentIdentityCredentialDelegate delegate(
      base::BindLambdaForTesting([&mock]() {
        content::FederatedAuthAutofillSource* result = &mock;
        return result;
      }));

  IdentityRequestAccountPtr account =
      base::MakeRefCounted<content::IdentityRequestAccount>(
          "id", "display_identifier", "display_name", "john@email.com", "name",
          "given_name", GURL(), "phone", "username",
          /*login_hints=*/std::vector<std::string>(),
          /*domain_hints=*/std::vector<std::string>(),
          /*labels=*/std::vector<std::string>());
  content::IdentityProviderMetadata metadata;
  metadata.config_url = GURL("https://idp.example");

  std::vector<content::IdentityRequestDialogDisclosureField> disclosures;

  scoped_refptr<content::IdentityProviderData> identity_provider =
      base::MakeRefCounted<content::IdentityProviderData>(
          "idp.example", metadata,
          content::ClientMetadata((GURL()), (GURL()), (GURL()), (gfx::Image())),
          blink::mojom::RpContext::kSignIn, disclosures, false);

  account->identity_provider = identity_provider;
  std::vector<IdentityRequestAccountPtr> accounts = {account};

  EXPECT_CALL(mock, GetAutofillSuggestions).WillOnce(Return(accounts));

  std::vector<Suggestion> suggestions =
      delegate.GetVerifiedAutofillSuggestions(PASSWORD);
  EXPECT_EQ(1ul, suggestions.size());

  Suggestion suggestion = suggestions[0];
  EXPECT_EQ(suggestion.main_text.value, u"john@email.com");
  EXPECT_EQ(suggestion.labels.size(), 1ul);
  EXPECT_EQ(suggestion.minor_texts.size(), 0ul);
}

}  // namespace

}  // namespace autofill
