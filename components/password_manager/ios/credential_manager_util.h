// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_IOS_CREDENTIAL_MANAGER_UTIL_H_
#define COMPONENTS_PASSWORD_MANAGER_IOS_CREDENTIAL_MANAGER_UTIL_H_

#include "base/values.h"
#include "components/password_manager/core/common/credential_manager_types.h"

namespace web {
class WebState;
}

namespace password_manager {

// Keys for obtaining common Credential's fields from DictionaryValue
// representing the Credential. Keys below correspond to JavaScript
// Credential object fields as follows:
// kCredentialIdKey   : |id|
// kCredentialTypeKey : |type|
// kCredentialNameKey : |name|
// kCredentialIconKey : |iconURL|
// Values under those keys are all strings.
extern const char kCredentialIdKey[];
extern const char kCredentialTypeKey[];
extern const char kCredentialNameKey[];
extern const char kCredentialIconKey[];

// Key for obtaining PasswordCredential's own property |password| from
// DictionaryValue representing the JavaScript object. Value under this key
// is a string.
extern const char kPasswordCredentialPasswordKey[];

// Key for obtaining FederatedCredential's own property |provider| from
// DictionaryValue representing the JavaScript object. Value under this key
// is a string.
extern const char kFederatedCredentialProviderKey[];

// Keys below correspond to JavaScript CredentialRequestOptions object fields
// as follows:
// kCredentialRequestMediationKey : |mediation|
// kCredentialRequestPasswordKey  : |password|
// kCredentialRequestProvidersKey : |providers|
// |mediation| value is a string, |password| value is a boolean and |providers|
// value should is a list of strings.
extern const char kCredentialRequestMediationKey[];
extern const char kCredentialRequestPasswordKey[];
extern const char kCredentialRequestProvidersKey[];

// Strings denoting acceptable CredentialRequestOptions.mediation values,
// respectively "silent", "required", "optional". Use them by comparing to
// obtained |mediation| value.
extern const char kMediationRequirementSilent[];
extern const char kMediationRequirementRequired[];
extern const char kMediationRequirementOptional[];

// Strings denoting acceptable Credential.type values, respectively
// "PasswordCredential", "FederatedCredential". Use them by comparing to
// obtained |type| value.
extern const char kCredentialTypePassword[];
extern const char kCredentialTypeFederated[];

// Returns value of Parse* methods below is false if |json| is invalid, which
// means it is missing required fields, contains fields of wrong type or
// unexpected values. Otherwise return value is true.
// Parses |mediation| field of JavaScript object CredentialRequestOptions.
bool ParseMediationRequirement(
    const base::DictionaryValue& json,
    password_manager::CredentialMediationRequirement* mediation);
// Parses |password| field of JavaScript object CredentialRequestOptions.
bool ParseIncludePasswords(const base::DictionaryValue& json,
                           bool* include_passwords);
// Parses |providers| field of JavaScript object
// FederatedCredentialRequestOptions into list of GURLs.
bool ParseFederations(const base::DictionaryValue& json,
                      std::vector<GURL>* federations);
// Parses |type| field from JavaScript Credential object into CredentialType.
bool ParseCredentialType(const base::DictionaryValue& json,
                         password_manager::CredentialType* credential_type);
// Parses dictionary representing JavaScript Credential object into
// CredentialInfo. If parsing fails, reason message is stored in |reason|.
// |reason| can be null, then it is ignored.
bool ParseCredentialDictionary(const base::DictionaryValue& json,
                               password_manager::CredentialInfo* credential,
                               std::string* reason);

// Checks if |web_state|'s content is a secure HTML. This is done in order to
// ignore API calls from insecure context.
bool WebStateContentIsSecureHtml(const web::WebState* web_state);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_IOS_CREDENTIAL_MANAGER_UTIL_H_
