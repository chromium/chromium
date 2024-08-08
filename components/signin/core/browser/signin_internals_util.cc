// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/signin_internals_util.h"

#include <sstream>

#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "crypto/sha2.h"
#include "google_apis/gaia/gaia_constants.h"

namespace signin_internals_util {

// Preference prefixes for signin and token values.
const char kSigninPrefPrefix[] = "google.services.signin.";

#define ENUM_CASE(x) case x: return (std::string(kSigninPrefPrefix) + #x)
std::string SigninStatusFieldToString(UntimedSigninStatusField field) {
  switch (field) {
    ENUM_CASE(ACCOUNT_ID);
    ENUM_CASE(GAIA_ID);
    ENUM_CASE(USERNAME);
  }

  NOTREACHED_IN_MIGRATION();
  return std::string();
}

std::string SigninStatusFieldToString(TimedSigninStatusField field) {
  switch (field) {
    ENUM_CASE(AUTHENTICATION_RESULT_RECEIVED);
    ENUM_CASE(REFRESH_TOKEN_RECEIVED);
    ENUM_CASE(LAST_SIGNIN_ACCESS_POINT);
    ENUM_CASE(LAST_SIGNOUT_SOURCE);
    case TIMED_FIELDS_END:
      NOTREACHED_IN_MIGRATION();
      return std::string();
  }

  NOTREACHED_IN_MIGRATION();
  return std::string();
}

} //  namespace signin_internals_util
