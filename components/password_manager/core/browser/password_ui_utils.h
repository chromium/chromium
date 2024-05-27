// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains utilities related to password manager's UI.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_UI_UTILS_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_UI_UTILS_H_

#include <string>
#include <utility>

#include "url/origin.h"

namespace password_manager {

class PasswordFormManagerForUI;
struct PasswordForm;
struct CredentialUIEntry;

// For Web credentials the returned origin is suitable for security display and
// is stripped off common prefixes like "m.", "mobile." or "www.".
//
//  For Android credentials the returned origin is set to the Play Store name
//  if available, otherwise it is the reversed package name (e.g.
//  com.example.android gets transformed to android.example.com).
std::string GetShownOrigin(const CredentialUIEntry& credential);
// Returns URL the full origin of the |credential|. For Android credential the
// link pints to affiliated website or to the Play Store if missing.
GURL GetShownUrl(const CredentialUIEntry& credential);

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
