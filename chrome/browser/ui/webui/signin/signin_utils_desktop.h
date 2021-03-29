// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_UTILS_DESKTOP_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_UTILS_DESKTOP_H_

#include <string>

class Profile;
class SigninUIError;

// Returns a non-error if sign-in is allowed for account with |email| and
// |gaia_id| to |profile|. If the sign-in is not allowed, then the error type
// and the error message are passed in the returned value.
SigninUIError CanOfferSignin(Profile* profile,
                             const std::string& gaia_id,
                             const std::string& email);

// Return true if the account given by |email| and |gaia_id| is signed in to
// Chrome in a different profile.
bool IsCrossAccountError(Profile* profile,
                         const std::string& email,
                         const std::string& gaia_id);

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_UTILS_DESKTOP_H_
