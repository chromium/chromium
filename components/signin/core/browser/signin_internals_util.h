// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_INTERNALS_UTIL_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_INTERNALS_UTIL_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>

#include "base/values.h"

namespace signin_internals_util {

// Preference prefixes for signin and token values.
extern const char kSigninPrefPrefix[];
extern const char kTokenPrefPrefix[];

// Helper enums to access fields from SigninStatus (declared below).
enum {
  SIGNIN_FIELDS_BEGIN = 0,
  UNTIMED_FIELDS_BEGIN_UNTYPED = SIGNIN_FIELDS_BEGIN
};

enum UntimedSigninStatusField {
  UNTIMED_FIELDS_BEGIN = UNTIMED_FIELDS_BEGIN_UNTYPED,
  ACCOUNT_ID = UNTIMED_FIELDS_BEGIN,
  GAIA_ID,
  USERNAME,
  UNTIMED_FIELDS_END
};

enum {
  UNTIMED_FIELDS_COUNT = UNTIMED_FIELDS_END - UNTIMED_FIELDS_BEGIN,
  TIMED_FIELDS_BEGIN_UNTYPED = UNTIMED_FIELDS_END
};

enum TimedSigninStatusField {
  TIMED_FIELDS_BEGIN = TIMED_FIELDS_BEGIN_UNTYPED,
  AUTHENTICATION_RESULT_RECEIVED = TIMED_FIELDS_BEGIN,
  REFRESH_TOKEN_RECEIVED,
  TIMED_FIELDS_END
};

enum {
  TIMED_FIELDS_COUNT = TIMED_FIELDS_END - TIMED_FIELDS_BEGIN,
  SIGNIN_FIELDS_END = TIMED_FIELDS_END,
  SIGNIN_FIELDS_COUNT = SIGNIN_FIELDS_END - SIGNIN_FIELDS_BEGIN
};

// Returns the name of a SigninStatus field.
std::string SigninStatusFieldToString(UntimedSigninStatusField field);
std::string SigninStatusFieldToString(TimedSigninStatusField field);

} // namespace signin_internals_util

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_INTERNALS_UTIL_H_
