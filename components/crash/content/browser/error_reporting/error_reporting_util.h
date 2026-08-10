// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRASH_CONTENT_BROWSER_ERROR_REPORTING_ERROR_REPORTING_UTIL_H_
#define COMPONENTS_CRASH_CONTENT_BROWSER_ERROR_REPORTING_ERROR_REPORTING_UTIL_H_

#include <string>

#include "base/component_export.h"

class GURL;

// Returns a string representation of `url` suitable for error reporting by
// stripping privacy-sensitive components, including username, password, query
// parameters, and fragment reference.
COMPONENT_EXPORT(JS_ERROR_REPORTING)
std::string RedactUrlForErrorReports(const GURL& url);

#endif  // COMPONENTS_CRASH_CONTENT_BROWSER_ERROR_REPORTING_ERROR_REPORTING_UTIL_H_
