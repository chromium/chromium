// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/signin_internals_util.h"

#include <sstream>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "crypto/sha2.h"
#include "google_apis/gaia/gaia_constants.h"

namespace signin_internals_util {

const char kSigninPrefPrefix[] = "google.services.signin.";
const char kTokenPrefPrefix[] = "google.services.signin.tokens.";

#define ENUM_CASE(x) case x: return (std::string(kSigninPrefPrefix) + #x)
std::string SigninStatusFieldToString(UntimedSigninStatusField field) {
  switch (field) {
    ENUM_CASE(ACCOUNT_ID);
    ENUM_CASE(GAIA_ID);
    ENUM_CASE(USERNAME);
    case UNTIMED_FIELDS_END:
      NOTREACHED();
      return std::string();
  }

  NOTREACHED();
  return std::string();
}

std::string SigninStatusFieldToString(TimedSigninStatusField field) {
  switch (field) {
    ENUM_CASE(AUTHENTICATION_RESULT_RECEIVED);
    ENUM_CASE(REFRESH_TOKEN_RECEIVED);
    case TIMED_FIELDS_END:
      NOTREACHED();
      return std::string();
  }

  NOTREACHED();
  return std::string();
}

} //  namespace signin_internals_util
