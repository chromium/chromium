// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains utilities related to password manager's UI.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_UI_UTILS_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_UI_UTILS_H_

#include <string>
#include <utility>

#include "base/strings/string_piece.h"

#include "url/origin.h"

namespace password_manager {

class PasswordFormManagerForUI;
struct PasswordForm;
struct CredentialUIEntry;
struct CredentialFacet;

// Reverses order of labels in hostname.
std::string SplitByDotAndReverse(base::StringPiece host);

// Returns a human readable origin and a link URL for the provided
// |password_form|.
//
// For Web credentials the returned origin is suitable for security display and
// is stripped off common prefixes like "m.", "mobile." or "www.". Furthermore
// the link URL is set to the full origin of the original form.
//
//  For Android credentials the returned origin is set to the Play Store name
//  if available, otherwise it is the reversed package name (e.g.
//  com.example.android gets transformed to android.example.com).
// TODO(crbug.com/1330906) Replace the usage with GetShownOrigin and GetShownUrl
std::pair<std::string, GURL> GetShownOriginAndLinkUrl(
    const PasswordForm& password_form);

// Together have the same result as |GetShownOriginAndLinkUrl| but works with
// |CredentialUIEntry|.
std::string GetShownOrigin(const CredentialUIEntry& credential);
GURL GetShownUrl(const CredentialUIEntry& credential);

// Equivalent to |GetShownOrigin(CredentialUIEntry)| but works with
// |CredentialFacet|.
std::string GetShownOrigin(const CredentialFacet& facet);

// Equivalent to |GetShownOriginAndLinkUrl| but works with |CredentialFacet|.
GURL GetShownUrl(const CredentialFacet& facet);

// Returns a string suitable for security display to the user (just like
// |FormatUrlForSecurityDisplay| with OMIT_HTTP_AND_HTTPS) based on origin of
// |password_form|) and without prefixes "m.", "mobile." or "www.".
std::string GetShownOrigin(const url::Origin& origin);

// Updates the |form_manager| pending credentials with |username| and
// |password|.
void UpdatePasswordFormUsernameAndPassword(
    const std::u16string& username,
    const std::u16string& password,
    PasswordFormManagerForUI* form_manager);

// Returns all the usernames for credentials saved for `signon_realm`. If
// `is_using_account_store` is true, this method will only consider
// credentials saved in the account store. Otherwise it will only consider
// credentials saved in the profile store.
std::vector<std::u16string> GetUsernamesForRealm(
    const std::vector<password_manager::CredentialUIEntry>& credentials,
    const std::string& signon_realm,
    bool is_using_account_store);

// Returns the resource identifier for the label describing the platform
// authenticator, e.g. "Use TouchID".
int GetPlatformAuthenticatorLabel();

// Returns the username or a label appropriate for display if it is empty.
std::u16string ToUsernameString(const std::u16string& username);
std::u16string ToUsernameString(const std::string& username);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_UI_UTILS_H_
