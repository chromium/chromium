// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRASH_CONTENT_BROWSER_ERROR_REPORTING_JAVASCRIPT_ERROR_REPORT_H_
#define COMPONENTS_CRASH_CONTENT_BROWSER_ERROR_REPORTING_JAVASCRIPT_ERROR_REPORT_H_

#include <string>

#include "base/optional.h"

// A report about a JavaScript error that we might want to send back to Google
// so it can be fixed. Fill in the fields and then call
// SendJavaScriptErrorReport.
struct JavaScriptErrorReport {
  JavaScriptErrorReport();
  JavaScriptErrorReport(const JavaScriptErrorReport& rhs);
  JavaScriptErrorReport(JavaScriptErrorReport&& rhs) noexcept;
  ~JavaScriptErrorReport();
  JavaScriptErrorReport& operator=(const JavaScriptErrorReport& rhs);
  JavaScriptErrorReport& operator=(JavaScriptErrorReport&& rhs) noexcept;

  // The error message. Must be present
  std::string message;

  // URL where the error occurred. Must be the full URL, containing the protocol
  // (e.g. http://www.example.com).
  std::string url;

  // Name of the product where the error occurred. If empty, use the product
  // variant of Chrome that is hosting the extension. (e.g. "Chrome" or
  // "Chrome_ChromeOS").
  std::string product;

  // Version of the product where the error occurred. If empty, use the version
  // of Chrome that is hosting the extension (e.g. "73.0.3683.75").
  std::string version;

  // Line number where the error occurred. Not sent if not present.
  base::Optional<int> line_number;

  // Column number where the error occurred. Not sent if not present.
  base::Optional<int> column_number;

  // String containing the stack trace for the error. Not sent if not present.
  base::Optional<std::string> stack_trace;
};

#endif  // COMPONENTS_CRASH_CONTENT_BROWSER_ERROR_REPORTING_JAVASCRIPT_ERROR_REPORT_H_
