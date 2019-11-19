// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_INTERNALS_LOGGING_SCOPE_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_INTERNALS_LOGGING_SCOPE_H_

namespace autofill {

class LogBuffer;

/////////////// Logging Scopes /////////////

// Generator for source code related to logging scopes. Pass a template T which
// takes a single parameter, the name of the logging scope.
#define AUTOFILL_LOGGING_SCOPE_TEMPLATES(T)                            \
  /* Information about the sync status, existence of profiles, etc. */ \
  T(Context)                                                           \
  /* Log messages related to the discovery and parsing of forms. */    \
  T(Parsing)                                                           \
  /* Log messages related to reasons to stop parsing a form. */        \
  T(AbortParsing)                                                      \
  /* Log messages related to filling of forms. */                      \
  T(Filling)                                                           \
  /* Log messages related to the submission of forms. */               \
  T(Submission)                                                        \
  /* Log messages related to communication with autofill server. */    \
  T(AutofillServer)

// Define a bunch of logging scopes: kContext, kParsing, ...
#define AUTOFILL_TEMPLATE(NAME) k##NAME,
enum class LoggingScope {
  AUTOFILL_LOGGING_SCOPE_TEMPLATES(AUTOFILL_TEMPLATE) kLastScope
};
#undef AUTOFILL_TEMPLATE

// Returns the enum value of |scope| as a string (without the leading k).
const char* LoggingScopeToString(LoggingScope scope);

LogBuffer& operator<<(LogBuffer& buf, LoggingScope scope);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_INTERNALS_LOGGING_SCOPE_H_
