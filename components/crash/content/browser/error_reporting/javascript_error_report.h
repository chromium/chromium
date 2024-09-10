// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRASH_CONTENT_BROWSER_ERROR_REPORTING_JAVASCRIPT_ERROR_REPORT_H_
#define COMPONENTS_CRASH_CONTENT_BROWSER_ERROR_REPORTING_JAVASCRIPT_ERROR_REPORT_H_

#include <optional>
#include <string>

#include "base/component_export.h"

// A report about a JavaScript error that we might want to send back to Google
// so it can be fixed. Fill in the fields and then call
// JsErrorReportProcessor::SendErrorReport().
struct COMPONENT_EXPORT(JS_ERROR_REPORTING) JavaScriptErrorReport {
  enum class WindowType {
    // No browser found, thus no window exists.
    kNoBrowser,
    // Valid window types.
    kRegularTabbed,
    kWebApp,
    kSystemWebApp,
  };

  JavaScriptErrorReport();
  JavaScriptErrorReport(const JavaScriptErrorReport& rhs);
  JavaScriptErrorReport(JavaScriptErrorReport&& rhs) noexcept;
  ~JavaScriptErrorReport();

  JavaScriptErrorReport& operator=(const JavaScriptErrorReport& rhs);
  JavaScriptErrorReport& operator=(JavaScriptErrorReport&& rhs) noexcept;

  // The error message. Must be present
  std::string message;

  // URL of the resource that threw or reported the error. Generally the URL of
  // the JavaScript, not the page's URL. Must include the protocol (e.g.
  // http://www.example.com/main.js) but not query, fragment, or other
  // privacy-sensitive details we don't want to send.
  std::string url;

  // The system that created the error report. Useful for checking that each of
  // the various different systems that generate JavaScript error reports is
  // working as expected.
  enum class SourceSystem {
    kUnknown,
    kCrashReportApi,
    kWebUIObserver,
    kDevToolsObserver,
  };
  SourceSystem source_system = SourceSystem::kUnknown;

  // Name of the product where the error occurred. If empty, use the product
  // variant of Chrome that is hosting the extension. (e.g. "Chrome" or
  // "Chrome_ChromeOS").
  std::string product;

  // Version of the product where the error occurred. If empty, use the version
  // of Chrome that is hosting the extension (e.g. "73.0.3683.75").
  std::string version;

  // Line number where the error occurred. Not sent if not present.
  std::optional<int> line_number;

  // Column number where the error occurred. Not sent if not present.
  std::optional<int> column_number;

  // String containing the stack trace for the error. Not sent if not present.
  std::optional<std::string> stack_trace;

  // String containing the application locale. Not sent if not present.
  std::optional<std::string> app_locale;

  // URL of the page the user was on when the error occurred. Must include the
  // protocol (e.g. http://www.example.com) but not query, fragment, or other
  // privacy-sensitive details we don't want to send.
  std::optional<std::string> page_url;

  // Some type of debug_id used to tie the JavaScript back to a source map in
  // the crash reporting backend. Matches the debug_id in the symbol upload
  // system.
  std::optional<std::string> debug_id;

  // Uptime of the renderer process in milliseconds. 0 if the callee
  // |web_contents| is null (shouldn't really happen as this is caled from a JS
  // context) or the renderer process doesn't exist (possible due to termination
  // / failure to start).
  int renderer_process_uptime_ms = 0;

  // The window type of the JS context that reported this error.
  std::optional<WindowType> window_type;

  // If true (the default), send this report to the production server. If false,
  // send to the staging server. This should be set to false for errors from
  // tests and dev builds, and true for real (end-user) errors.
  bool send_to_production_servers = true;
};

#endif  // COMPONENTS_CRASH_CONTENT_BROWSER_ERROR_REPORTING_JAVASCRIPT_ERROR_REPORT_H_
