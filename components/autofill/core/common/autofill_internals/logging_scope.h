// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_INTERNALS_LOGGING_SCOPE_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_INTERNALS_LOGGING_SCOPE_H_

namespace autofill {

class LogBuffer;

/////////////// Logging Scopes /////////////

// Generator for source code related to logging scopes. Pass a template T which
// takes a single parameter, the name of the scope the log messages are related
// to.
#define AUTOFILL_LOGGING_SCOPE_TEMPLATES(T)                            \
  /* Information about the sync status, existence of profiles, etc. */ \
  T(Context)                                                           \
  /* Discovery and parsing of forms. */                                \
  T(Parsing)                                                           \
  /* Rationalization-induced changes to parsing. */                    \
  T(Rationalization)                                                   \
  /* Reasons to stop parsing a form. */                                \
  T(AbortParsing)                                                      \
  /* Filling of forms. */                                              \
  T(Filling)                                                           \
  /* Submission of forms. */                                           \
  T(Submission)                                                        \
  /* Communication with autofill server. */                            \
  T(AutofillServer)                                                    \
  /* Metrics collection. */                                            \
  T(Metrics)                                                           \
  /* Import of address profiles from form submissions. */              \
  T(AddressProfileFormImport)                                          \
  /* If credit card upload is either enabled or disabled. */           \
  T(CreditCardUploadStatus)                                            \
  /* Whether or not card upload was offered to the user. */            \
  T(CardUploadDecision)                                                \
  /* The website modified a field */                                   \
  T(WebsiteModifiedFieldValue)                                         \
  /* Chrome Fast Checkout run. */                                      \
  T(FastCheckout)                                                      \
  /* Touch To Fill UI. */                                              \
  T(TouchToFill)

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
