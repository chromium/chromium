// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains utilities related to password manager's UI.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_UI_UTILS_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_UI_UTILS_H_

#include <string>
#include <utility>

#include "base/strings/string_piece.h"

#include "url/gurl.h"

namespace autofill {
struct PasswordForm;
}

namespace password_manager {

class PasswordFormManagerForUI;

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
std::pair<std::string, GURL> GetShownOriginAndLinkUrl(
    const autofill::PasswordForm& password_form);

// Returns a string suitable for security display to the user (just like
// |FormatUrlForSecurityDisplay| with OMIT_HTTP_AND_HTTPS) based on origin of
// |password_form|) and without prefixes "m.", "mobile." or "www.".
std::string GetShownOrigin(const GURL& origin);

// Updates the |form_manager| pending credentials with |username| and
// |password|.
void UpdatePasswordFormUsernameAndPassword(
    const base::string16& username,
    const base::string16& password,
    PasswordFormManagerForUI* form_manager);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_UI_UTILS_H_
