// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/ios/credential_manager_util.h"

#include "components/security_state/ios/security_state_utils.h"
#import "ios/web/common/origin_util.h"
#import "ios/web/public/web_state.h"
#include "url/origin.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace password_manager {

const char kCredentialIdKey[] = "_id";
const char kCredentialTypeKey[] = "_type";
const char kCredentialNameKey[] = "_name";
const char kCredentialIconKey[] = "_iconURL";
const char kPasswordCredentialPasswordKey[] = "_password";
const char kFederatedCredentialProviderKey[] = "_provider";
const char kCredentialRequestMediationKey[] = "mediation";
const char kCredentialRequestPasswordKey[] = "password";
const char kCredentialRequestProvidersKey[] = "providers";
const char kMediationRequirementSilent[] = "silent";
const char kMediationRequirementRequired[] = "required";
const char kMediationRequirementOptional[] = "optional";
const char kCredentialTypePassword[] = "password";
const char kCredentialTypeFederated[] = "federated";

bool ParseMediationRequirement(const base::DictionaryValue& json,
                               CredentialMediationRequirement* mediation) {
  if (!json.HasKey(kCredentialRequestMediationKey)) {
    *mediation = CredentialMediationRequirement::kOptional;
    return true;
  }
  std::string mediation_str;
  if (!json.GetString(kCredentialRequestMediationKey, &mediation_str)) {
    // Dictionary contains |mediation| field but it is not a valid string.
    return false;
  }
  if (mediation_str == kMediationRequirementSilent) {
    *mediation = CredentialMediationRequirement::kSilent;
    return true;
  }
  if (mediation_str == kMediationRequirementRequired) {
    *mediation = CredentialMediationRequirement::kRequired;
    return true;
  }
  if (mediation_str == kMediationRequirementOptional) {
    *mediation = CredentialMediationRequirement::kOptional;
    return true;
  }
  // |mediation| value is not in {"silent", "required", "optional"}.
  // Dictionary is invalid then.
  return false;
}

bool ParseIncludePasswords(const base::DictionaryValue& json,
                           bool* include_passwords) {
  if (!json.HasKey(kCredentialRequestPasswordKey)) {
    *include_passwords = false;
    return true;
  }
  return json.GetBoolean(kCredentialRequestPasswordKey, include_passwords);
}

bool ParseFederations(const base::DictionaryValue& json,
                      std::vector<GURL>* federations) {
  federations->clear();
  if (!json.HasKey(kCredentialRequestProvidersKey)) {
    // No |providers| list.
    return true;
  }
  const base::ListValue* lst = nullptr;
  if (!json.GetList(kCredentialRequestProvidersKey, &lst)) {
    // Dictionary has |providers| field but it is not a list. That means
    // dictionary is invalid.
    return false;
  }
  for (size_t i = 0; i < lst->GetSize(); i++) {
    std::string s;
    if (!lst->GetString(i, &s)) {
      // Element of |providers| is invalid string.
      return false;
    }
    GURL gurl(s);
    if (gurl.is_valid()) {
      // Skip the invalid URLs. See
      // https://w3c.github.io/webappsec-credential-management/#provider-identification
      federations->push_back(std::move(gurl));
    }
  }
  return true;
}

bool ParseCredentialType(const base::DictionaryValue& json,
                         CredentialType* credential_type) {
  std::string str;
  if (!json.GetString(kCredentialTypeKey, &str)) {
    // Credential must contain |type|
    return false;
  }
  if (str == kCredentialTypePassword) {
    *credential_type = CredentialType::CREDENTIAL_TYPE_PASSWORD;
    return true;
  }
  if (str == kCredentialTypeFederated) {
    *credential_type = CredentialType::CREDENTIAL_TYPE_FEDERATED;
    return true;
  }
  return false;
}

bool ParseCredentialDictionary(const base::DictionaryValue& json,
                               CredentialInfo* credential,
                               std::string* reason) {
  base::string16 id;
  if (!json.GetString(kCredentialIdKey, &id)) {
    // |id| is required.
    if (reason) {
      *reason = "no valid 'id' field";
    }
    return false;
  }
  credential->id = id;

  base::string16 name;
  json.GetString(kCredentialNameKey, &name);
  credential->name = name;

  std::string iconUrl;
  if (json.GetString(kCredentialIconKey, &iconUrl) && !iconUrl.empty()) {
    credential->icon = GURL(iconUrl);
    if (!credential->icon.is_valid() ||
        !web::IsOriginSecure(credential->icon)) {
      // |iconUrl| is either not a valid URL or not a secure URL.
      if (reason) {
        *reason = "iconURL is either invalid or insecure URL";
      }
      return false;
    }
  }
  if (!ParseCredentialType(json, &credential->type)) {
    // Credential has invalid |type|
    if (reason) {
      *reason = "Credential has invalid type";
    }
    return false;
  }
  if (credential->type == CredentialType::CREDENTIAL_TYPE_PASSWORD) {
    base::string16 password;
    if (!json.GetString(kPasswordCredentialPasswordKey, &password) ||
        password.empty()) {
      // |password| field is required for PasswordCredential.
      if (reason) {
        *reason = "no valid 'password' field";
      }
      return false;
    }
    credential->password = password;
  }
  if (credential->type == CredentialType::CREDENTIAL_TYPE_FEDERATED) {
    std::string federation;
    json.GetString(kFederatedCredentialProviderKey, &federation);
    if (!GURL(federation).is_valid()) {
      // |provider| field must be a valid URL. See
      // https://w3c.github.io/webappsec-credential-management/#provider-identification
      if (reason) {
        *reason = "no valid 'provider' field";
      }
      return false;
    }
    credential->federation = url::Origin::Create(GURL(federation));
  }
  return true;
}

bool WebStateContentIsSecureHtml(const web::WebState* web_state) {
  if (!web_state) {
    return false;
  }

  if (!web_state->ContentIsHTML()) {
    return false;
  }

  const GURL last_committed_url = web_state->GetLastCommittedURL();

  if (!web::IsOriginSecure(last_committed_url) ||
      last_committed_url.scheme() == url::kDataScheme) {
    return false;
  }

  // If scheme is not cryptographic, the origin must be either localhost or a
  // file.
  if (!security_state::IsSchemeCryptographic(last_committed_url)) {
    return security_state::IsOriginLocalhostOrFile(last_committed_url);
  }

  // If scheme is cryptographic, valid SSL certificate is required.
  security_state::SecurityLevel security_level =
      security_state::GetSecurityLevelForWebState(web_state);
  return security_state::IsSslCertificateValid(security_level);
}

}  // namespace password_manager
