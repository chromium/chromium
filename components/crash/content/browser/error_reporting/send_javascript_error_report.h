// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRASH_CONTENT_BROWSER_ERROR_REPORTING_SEND_JAVASCRIPT_ERROR_REPORT_H_
#define COMPONENTS_CRASH_CONTENT_BROWSER_ERROR_REPORTING_SEND_JAVASCRIPT_ERROR_REPORT_H_

#include <stdint.h>

#include <string>

#include "base/callback_forward.h"
#include "build/build_config.h"

namespace content {
class BrowserContext;
}
struct JavaScriptErrorReport;

// TODO(crbug.com/1129544) This is currently disabled due to Windows DLL
// thunking issues. Fix & re-enable.
#if !defined(OS_WIN)

// Sends a report of an error in JavaScript (such as an unhandled exception) to
// Google's error collection service. This should be called on the UI thread;
// it will return after the report sending is started. |completion_callback| is
// called when the report send completes or fails.
void SendJavaScriptErrorReport(JavaScriptErrorReport error_report,
                               base::OnceClosure completion_callback,
                               content::BrowserContext* browser_context);

#endif  // !defined(OS_WIN)

// Override the URL we send the crashes to.
void SetCrashEndpointForTesting(const std::string& endpoint);

// Override the OS Version.
void SetOsVersionForTesting(int32_t os_major_version,
                            int32_t os_minor_version,
                            int32_t os_bugfix_version);
// Go back to the default behavior of getting the OS version from the OS.
void ClearOsVersionTestingOverride();

#endif  // COMPONENTS_CRASH_CONTENT_BROWSER_ERROR_REPORTING_SEND_JAVASCRIPT_ERROR_REPORT_H_
