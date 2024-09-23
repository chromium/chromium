// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_OS_REGISTRATION_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_OS_REGISTRATION_H_

#include <string_view>
#include <vector>

#include "base/component_export.h"
#include "base/types/expected.h"
#include "components/attribution_reporting/os_registration_error.mojom-forward.h"
#include "net/http/structured_headers.h"
#include "url/gurl.h"

namespace attribution_reporting {

struct COMPONENT_EXPORT(ATTRIBUTION_REPORTING) OsRegistrationItem {
  GURL url;
  bool debug_reporting = false;

  friend bool operator==(const OsRegistrationItem&,
                         const OsRegistrationItem&) = default;
};

// Parses an Attribution-Reporting-OS-Source or
// Attribution-Reporting-Register-OS-Trigger header.
//
// Returns `OsRegistrationError` if the string is not parsable as a
// structured-header list. List members that are not strings or do not contain a
// valid URL are ignored.
//
// Example:
//
// "https://x.test/abc", "https://y.test/123"
COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
base::expected<std::vector<OsRegistrationItem>, mojom::OsRegistrationError>
    ParseOsSourceOrTriggerHeader(std::string_view);

// Same as the above, but using an already-parsed structured-header list.
COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
base::expected<std::vector<OsRegistrationItem>, mojom::OsRegistrationError>
ParseOsSourceOrTriggerHeader(const net::structured_headers::List&);

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_OS_REGISTRATION_H_
