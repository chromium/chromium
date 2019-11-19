// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_internals/logging_scope.h"

#include "base/logging.h"
#include "components/autofill/core/common/logging/log_buffer.h"

namespace autofill {

const char* LoggingScopeToString(LoggingScope scope) {
  // Define template to generate case-statements:
#define AUTOFILL_TEMPLATE(NAME) \
  case LoggingScope::k##NAME:   \
    return #NAME;

  // The actual implementation of this function.
  switch (scope) {
    AUTOFILL_LOGGING_SCOPE_TEMPLATES(AUTOFILL_TEMPLATE)
    case LoggingScope::kLastScope:
      return "";
      // No default here to cover all cases.
  }

    // Clean up.
#undef AUTOFILL_TEMPLATE

  NOTREACHED();
  return "";
}

LogBuffer& operator<<(LogBuffer& buf, LoggingScope scope) {
  if (!buf.active())
    return buf;
  return buf << Tag{"div"} << Attrib{"scope", LoggingScopeToString(scope)}
             << Attrib{"class", "log-entry"};
}

}  // namespace autofill
