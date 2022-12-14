// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATTRIBUTION_REPORTING_OS_REGISTRATION_H_
#define COMPONENTS_ATTRIBUTION_REPORTING_OS_REGISTRATION_H_

#include "base/component_export.h"
#include "base/strings/string_piece_forward.h"
#include "components/attribution_reporting/os_support.mojom-forward.h"
#include "url/gurl.h"

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

COMPONENT_EXPORT(ATTRIBUTION_REPORTING)
base::StringPiece GetSupportHeader(mojom::OsSupport);

}  // namespace attribution_reporting

#endif  // COMPONENTS_ATTRIBUTION_REPORTING_OS_REGISTRATION_H_
