// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/ios/credential_manager_util.h"

#include <memory>

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace password_manager {

namespace {

constexpr char kTestWebOrigin[] = "https://example.com/";
constexpr char kTestIconUrl[] = "https://www.google.com/favicon.ico";

base::DictionaryValue BuildExampleCredential() {
  base::DictionaryValue json;
  json.SetString(kCredentialIdKey, "john@doe.com");
  json.SetString(kCredentialNameKey, "John Doe");
  json.SetString(kCredentialIconKey, kTestIconUrl);
  return json;
}

// Builds a dictionary representing valid PasswordCredential.
base::DictionaryValue BuildExampleValidPasswordCredential() {
  base::DictionaryValue json = BuildExampleCredential();
  json.SetString(kPasswordCredentialPasswordKey, "admin123");
  json.SetString(kCredentialTypeKey, kCredentialTypePassword);
  return json;
}

// Builds a dictionary representing valid FederatedCredential.
base::DictionaryValue BuildExampleValidFederatedCredential() {
  base::DictionaryValue json = BuildExampleCredential();
  json.SetString(kFederatedCredentialProviderKey, kTestWebOrigin);
  json.SetString(kCredentialTypeKey, kCredentialTypeFederated);
  return json;
}

}  // namespace

using CredentialManagerUtilTest = PlatformTest;

// Checks that CredentialRequestOptions.password field is parsed
// correctly.
TEST_F(CredentialManagerUtilTest, ParseIncludePasswords) {
  base::DictionaryValue json;
  bool include_passwords = true;

  // Default value should be false.
  EXPECT_TRUE(ParseIncludePasswords(json, &include_passwords));
  EXPECT_FALSE(include_passwords);

  // true/false values should be parsed correctly.
  json.SetBoolean(kCredentialRequestPasswordKey, true);
  EXPECT_TRUE(ParseIncludePasswords(json, &include_passwords));
  EXPECT_TRUE(include_passwords);

  json.SetBoolean(kCredentialRequestPasswordKey, false);
  EXPECT_TRUE(ParseIncludePasswords(json, &include_passwords));
  EXPECT_FALSE(include_passwords);

  // Test against random string.
  json.SetString(kCredentialRequestPasswordKey, "yes");
  EXPECT_FALSE(ParseIncludePasswords(json, &include_passwords));
}

// Checks that CredentialRequestOptions.mediation field is parsed
// correctly.
TEST_F(CredentialManagerUtilTest, ParseMediationRequirement) {
  base::DictionaryValue json;
  CredentialMediationRequirement mediation;

  // Default value should be kOptional.
  EXPECT_TRUE(ParseMediationRequirement(json, &mediation));
  EXPECT_EQ(CredentialMediationRequirement::kOptional, mediation);

  // "silent"/"optional"/"required" values should be parsed correctly.
  json.SetString(kCredentialRequestMediationKey, kMediationRequirementSilent);
  EXPECT_TRUE(ParseMediationRequirement(json, &mediation));
  EXPECT_EQ(CredentialMediationRequirement::kSilent, mediation);

  json.SetString(kCredentialRequestMediationKey, kMediationRequirementOptional);
  EXPECT_TRUE(ParseMediationRequirement(json, &mediation));
  EXPECT_EQ(CredentialMediationRequirement::kOptional, mediation);

  json.SetString(kCredentialRequestMediationKey, kMediationRequirementRequired);
  EXPECT_TRUE(ParseMediationRequirement(json, &mediation));
  EXPECT_EQ(CredentialMediationRequirement::kRequired, mediation);

  // Test against random string.
  json.SetString(kCredentialRequestMediationKey, "dksjl");
  EXPECT_FALSE(ParseMediationRequirement(json, &mediation));
}

// Checks that Credential.type field is parsed correctly.
TEST_F(CredentialManagerUtilTest, ParseCredentialType) {
  base::DictionaryValue json;
  CredentialType type = CredentialType::CREDENTIAL_TYPE_EMPTY;

  // JS object Credential must contain |type| field.
  EXPECT_FALSE(ParseCredentialType(json, &type));

  // "PasswordCredential"/"FederatedCredential" values should be parsed
  // correctly.
  json.SetString(kCredentialTypeKey, kCredentialTypePassword);
  EXPECT_TRUE(ParseCredentialType(json, &type));
  EXPECT_EQ(CredentialType::CREDENTIAL_TYPE_PASSWORD, type);

  json.SetString(kCredentialTypeKey, kCredentialTypeFederated);
  EXPECT_TRUE(ParseCredentialType(json, &type));
  EXPECT_EQ(CredentialType::CREDENTIAL_TYPE_FEDERATED, type);

  // "Credential" is not a valid type.
  json.SetString(kCredentialTypeKey, "Credential");
  EXPECT_FALSE(ParseCredentialType(json, &type));

  // Empty string is also not allowed.
  json.SetString(kCredentialTypeKey, "");
  EXPECT_FALSE(ParseCredentialType(json, &type));
}

// Checks that common fields of PasswordCredential and FederatedCredential are
// parsed correctly.
TEST_F(CredentialManagerUtilTest, ParseCommonCredentialFields) {
  // Building PasswordCredential because ParseCredentialDictionary for
  // Credential containing only common fields would return false.
  base::DictionaryValue json = BuildExampleValidPasswordCredential();
  CredentialInfo credential;
  std::string reason;

  // Valid dictionary should be parsed correctly and ParseCredentialDictionary
  // should return true.
  EXPECT_TRUE(ParseCredentialDictionary(json, &credential, &reason));
  EXPECT_EQ(base::ASCIIToUTF16("john@doe.com"), credential.id);
  EXPECT_EQ(base::ASCIIToUTF16("John Doe"), credential.name);
  EXPECT_EQ(GURL(kTestIconUrl), credential.icon);

  // |id| field is required.
  json.Remove(kCredentialIdKey, nullptr);
  EXPECT_FALSE(ParseCredentialDictionary(json, &credential, &reason));
  EXPECT_EQ(reason, "no valid 'id' field");

  // |iconURL| field is not required.
  json = BuildExampleValidPasswordCredential();
  json.Remove(kCredentialIconKey, nullptr);
  EXPECT_TRUE(ParseCredentialDictionary(json, &credential, &reason));

  // If Credential has |iconURL| field, it must be a valid URL.
  json.SetString(kCredentialIconKey, "not a valid url");
  EXPECT_FALSE(ParseCredentialDictionary(json, &credential, &reason));
  EXPECT_EQ(reason, "iconURL is either invalid or insecure URL");

  // If Credential has |iconURL| field, it must be a secure URL.
  reason = std::string();
  json.SetString(kCredentialIconKey, "http://example.com");
  EXPECT_FALSE(ParseCredentialDictionary(json, &credential, &reason));
  EXPECT_EQ(reason, "iconURL is either invalid or insecure URL");

  // Check that empty |iconURL| field is treated as no |iconURL| field.
  json.SetString(kCredentialIconKey, "");
  EXPECT_TRUE(ParseCredentialDictionary(json, &credential, &reason));
}

// Checks that |password| and |type| fields of PasswordCredential are parsed
// correctly.
TEST_F(CredentialManagerUtilTest, ParsePasswordCredential) {
  base::DictionaryValue json = BuildExampleValidPasswordCredential();
  CredentialInfo credential;
  std::string reason;

  // Valid dictionary should be parsed correctly and ParseCredentialDictionary
  // should return true.
  EXPECT_TRUE(ParseCredentialDictionary(json, &credential, &reason));
  EXPECT_EQ(CredentialType::CREDENTIAL_TYPE_PASSWORD, credential.type);
  EXPECT_EQ(base::ASCIIToUTF16("admin123"), credential.password);

  // |password| field is required.
  json.Remove(kPasswordCredentialPasswordKey, nullptr);
  EXPECT_FALSE(ParseCredentialDictionary(json, &credential, &reason));
  EXPECT_EQ(reason, "no valid 'password' field");

  // |password| field is cannot be an empty string.
  json.SetString(kPasswordCredentialPasswordKey, "");
  EXPECT_FALSE(ParseCredentialDictionary(json, &credential, &reason));
  EXPECT_EQ(reason, "no valid 'password' field");
}

// Checks that |provider| and |type| fields of FederatedCredential are parsed
// correctly.
TEST_F(CredentialManagerUtilTest, ParseFederatedCredential) {
  base::DictionaryValue json = BuildExampleValidFederatedCredential();
  CredentialInfo credential;
  std::string reason;

  // Valid dictionary should be parsed correctly and ParseCredentialDictionary
  // should return true.
  EXPECT_TRUE(ParseCredentialDictionary(json, &credential, &reason));
  EXPECT_EQ(CredentialType::CREDENTIAL_TYPE_FEDERATED, credential.type);
  EXPECT_EQ(GURL(kTestWebOrigin), credential.federation.GetURL());

  // |provider| field is required.
  json.Remove(kFederatedCredentialProviderKey, nullptr);
  EXPECT_FALSE(ParseCredentialDictionary(json, &credential, &reason));
  EXPECT_EQ(reason, "no valid 'provider' field");

  // |provider| field cannot be an empty string.
  json.SetString(kFederatedCredentialProviderKey, "");
  EXPECT_FALSE(ParseCredentialDictionary(json, &credential, &reason));
  EXPECT_EQ(reason, "no valid 'provider' field");

  // |provider| field must be a valid URL.
  json.SetString(kFederatedCredentialProviderKey, "not a valid URL");
  EXPECT_FALSE(ParseCredentialDictionary(json, &credential, &reason));
  EXPECT_EQ(reason, "no valid 'provider' field");
}

// Checks that |providers| field of FederatedCredentialRequestOptions is
// parsed correctly.
TEST_F(CredentialManagerUtilTest, ParseFederations) {
  base::DictionaryValue json;

  // Build example valid |providers| list.
  std::unique_ptr<base::ListValue> list_ptr =
      std::make_unique<base::ListValue>();
  list_ptr->Append(kTestWebOrigin);
  list_ptr->Append("https://google.com");
  json.SetList(kCredentialRequestProvidersKey, std::move(list_ptr));
  std::vector<GURL> federations;

  // Check that parsing valid |providers| results in correct |federations| list.
  EXPECT_TRUE(ParseFederations(json, &federations));
  EXPECT_THAT(federations, testing::ElementsAre(GURL(kTestWebOrigin),
                                                GURL("https://google.com")));

  // ParseFederations should skip invalid URLs.
  list_ptr = std::make_unique<base::ListValue>();
  list_ptr->Append(kTestWebOrigin);
  list_ptr->Append("not a valid url");
  json.SetList(kCredentialRequestProvidersKey, std::move(list_ptr));
  EXPECT_TRUE(ParseFederations(json, &federations));
  EXPECT_THAT(federations, testing::ElementsAre(GURL(kTestWebOrigin)));

  // If |providers| is not a valid list, ParseFederations should return false.
  json.SetString(kCredentialRequestProvidersKey, kTestWebOrigin);
  EXPECT_FALSE(ParseFederations(json, &federations));
}

}  // namespace password_manager
