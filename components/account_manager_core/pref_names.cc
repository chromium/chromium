// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/account_manager_core/pref_names.h"

namespace account_manager {
namespace prefs {

// A boolean pref to store if Secondary Google Account additions are allowed on
// Chrome OS Account Manager. The default value is |true|, i.e. Secondary Google
// Account additions are allowed by default.
const char kSecondaryGoogleAccountSigninAllowed[] =
    "account_manager.secondary_google_account_signin_allowed";

}  // namespace prefs
}  // namespace account_manager
