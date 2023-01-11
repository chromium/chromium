// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRASH_CONTENT_BROWSER_ERROR_REPORTING_JS_ERROR_REPORT_PROCESSOR_H_
#define COMPONENTS_CRASH_CONTENT_BROWSER_ERROR_REPORTING_JS_ERROR_REPORT_PROCESSOR_H_

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"

namespace content {
class BrowserContext;
}
struct JavaScriptErrorReport;

// Interface class that exposes the SendErrorReport function.
// We use RefCountedThreadSafe instead of the more normal RefCounted or WeakPtrs
// because multiple reports can be in-flight at the same time, each on a
// different sequence, but still using the same JsErrorReportProcessor.
class COMPONENT_EXPORT(JS_ERROR_REPORTING) JsErrorReportProcessor
    : public base::RefCountedThreadSafe<JsErrorReportProcessor> {
 public:
  REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE();
  // Returns the current implementation of JsErrorReportProcessor. Callers
  // should check for nullptr.
  static scoped_refptr<JsErrorReportProcessor> Get();

  // Sends a report of an error in JavaScript (such as an unhandled exception)
  // to Google's error collection service. This should be called on the UI
  // thread; it will return after the report sending is started.
  // |completion_callback| is called when the report send completes or fails.
  virtual void SendErrorReport(JavaScriptErrorReport error_report,
                               base::OnceClosure completion_callback,
                               content::BrowserContext* browser_context) = 0;

 protected:
  friend class base::RefCountedThreadSafe<JsErrorReportProcessor>;
  JsErrorReportProcessor();
  virtual ~JsErrorReportProcessor();

  // Sets the JsErrorReportProcessor pointer than should be returned by Get().
  static void SetDefault(scoped_refptr<JsErrorReportProcessor> processor);
};

#endif  // COMPONENTS_CRASH_CONTENT_BROWSER_ERROR_REPORTING_JS_ERROR_REPORT_PROCESSOR_H_
