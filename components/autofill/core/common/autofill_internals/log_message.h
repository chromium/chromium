// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_INTERNALS_LOG_MESSAGE_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_INTERNALS_LOG_MESSAGE_H_

namespace autofill {

class LogBuffer;

/////////////// Log Messages /////////////

// Generator for log message. If you need to find the call site for a log
// message, take the first parameter (e.g. ParsedForms) and search for
// that name prefixed with a k (e.g. kParsedForms) in code search.
#define AUTOFILL_LOG_MESSAGE_TEMPLATES(T)                                      \
  T(ParsedForms, "Parsed forms:")                                              \
  T(SendAutofillUpload, "Sending Autofill Upload Request:")                    \
  T(LocalHeuristicRegExMatched, "RegEx of local heuristic matched:")           \
  T(AbortParsingTooManyForms, "Abort parsing form: Too many forms in cache: ") \
  T(AbortParsingNotAllowedScheme,                                              \
    "Abort parsing form: Ignoring form because the source url has no allowed " \
    "scheme")                                                                  \
  T(AbortParsingNotEnoughFields,                                               \
    "Abort parsing form: Not enough fields in form: ")                         \
  T(AbortParsingUrlMatchesSearchRegex,                                         \
    "Abort parsing form: Action URL matches kUrlSearchActionRe, indicating "   \
    "that the form may lead to a search.")                                     \
  T(AbortParsingFormHasNoTextfield,                                            \
    "Abort parsing form: Form has no text field.")

// Log messages for chrome://autofill-internals.
#define AUTOFILL_TEMPLATE(NAME, MESSAGE) k##NAME,
enum class LogMessage {
  AUTOFILL_LOG_MESSAGE_TEMPLATES(AUTOFILL_TEMPLATE) kLastMessage
};
#undef AUTOFILL_TEMPLATE

// Returns the enum value of |message| as a string (without the leading k).
const char* LogMessageToString(LogMessage message);
// Returns the actual string to be presented to the user for |message|.
const char* LogMessageValue(LogMessage message);

LogBuffer& operator<<(LogBuffer& buf, LogMessage message);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_INTERNALS_LOG_MESSAGE_H_
