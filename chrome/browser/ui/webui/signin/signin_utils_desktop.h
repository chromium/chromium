// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_UTILS_DESKTOP_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_UTILS_DESKTOP_H_

#include <string>

class GaiaId;
class Profile;
class SigninUIError;

// Returns a non-error if sign-in is allowed for account with |email| and
// |gaia_id| to |profile|. If the sign-in is not allowed, then the error type
// and the error message are passed in the returned value.
// This function can be used either for new signins or for reauthentication of
// an already existing account. In the case of reauth, the function checks that
// the account being reauthenticated matches the current account.
// If `allow_account_from_other_profile` is false, then the function checks if
// another profile is already signed in with the same account, and returns an
// error if this is the case.
SigninUIError CanOfferSignin(Profile* profile,
                             const GaiaId& gaia_id,
                             const std::string& email,
                             bool allow_account_from_other_profile);

// Return true if an account other than `gaia_id` was previously signed into
// `profile`.
bool IsCrossAccountError(Profile* profile, const GaiaId& gaia_id);

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_UTILS_DESKTOP_H_
