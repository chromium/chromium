// Copyright 2017 The Chromium Authors
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
// This function can be used either for new signins or for reauthentication of
// an already existing account. In the case of reauth, the function checks that
// the account being reauthenticated matches the current Sync account.
// TODO(alexilin): consider renaming this function to CanOfferSyncOrReauth() or
// similar to make it clear that this function is about signin into Sync.
SigninUIError CanOfferSignin(Profile* profile,
                             const std::string& gaia_id,
                             const std::string& email);

// Return true if an account other than `gaia_id` was previously signed into
// `profile`.
bool IsCrossAccountError(Profile* profile, const std::string& gaia_id);

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNIN_UTILS_DESKTOP_H_
