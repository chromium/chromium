// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_OS_REGISTRATION_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_OS_REGISTRATION_H_

#include "base/component_export.h"
#include "base/strings/string_piece_forward.h"
#include "net/http/structured_headers.h"

class GURL;

namespace attribution_reporting {

// Parses an Attribution-Reporting-OS-Source or
// Attribution-Reporting-Register-OS-Trigger header.
//
// The structured-header item may have parameters, but they are ignored.
//
// Returns an invalid `GURL` if `header` is not parsable as a structured-header
// item, if the item is not a string, or if the string is not a valid URL.
//
// Example:
//
// "https://x.test/abc"
COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
GURL ParseOsSourceOrTriggerHeader(base::StringPiece);

// Same as the above, but using an already-parsed structured-header item.
COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
GURL ParseOsSourceOrTriggerHeader(
    const net::structured_headers::ParameterizedItem&);

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_OS_REGISTRATION_H_
