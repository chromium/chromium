// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_INTERNALS_UTIL_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_INTERNALS_UTIL_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>


namespace signin_internals_util {

enum UntimedSigninStatusField { ACCOUNT_ID, GAIA_ID, USERNAME };

enum TimedSigninStatusField {
  TIMED_FIELDS_BEGIN,
  AUTHENTICATION_RESULT_RECEIVED = TIMED_FIELDS_BEGIN,
  REFRESH_TOKEN_RECEIVED,
  LAST_SIGNIN_ACCESS_POINT,
  LAST_SIGNOUT_SOURCE,
  TIMED_FIELDS_END
};

// Returns the name of a SigninStatus field.
std::string SigninStatusFieldToString(UntimedSigninStatusField field);
std::string SigninStatusFieldToString(TimedSigninStatusField field);

} // namespace signin_internals_util

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_INTERNALS_UTIL_H_
