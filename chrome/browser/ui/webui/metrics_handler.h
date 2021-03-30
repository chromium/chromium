// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_METRICS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_METRICS_HANDLER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/public/browser/web_ui_message_handler.h"

///////////////////////////////////////////////////////////////////////////////
// MetricsHandler

// Let the page contents record UMA actions. Only use when you can't do it from
// C++. For example, we currently use it to let the NTP log the position of the
// Most Visited or Bookmark the user clicked on, as we don't get that
// information through RequestOpenURL. You will need to update the metrics
// dashboard with the action names you use, as our processor won't catch that
// information (treat it as RecordComputedMetrics)

namespace base {
class ListValue;
}

class MetricsHandler : public content::WebUIMessageHandler {
 public:
  MetricsHandler();
  ~MetricsHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  // Callback for the "metricsHandler:recordAction" message. This records a
  // user action.
  void HandleRecordAction(const base::ListValue* args);

  // TODO(dbeam): http://crbug.com/104338

  // Callback for the "metricsHandler:recordInHistogram" message. This records
  // into a histogram. |args| contains the histogram name, the value to record,
  // and the maximum allowed value, which can be at most 4000. The histogram
  // will use at most 100 buckets, one for each 1, 10, or 100 different values,
  // depending on the maximum value.
  void HandleRecordInHistogram(const base::ListValue* args);

  // Callback for the "metricsHandler:recordBooleanHistogram" message. This
  // records into a boolean histogram. |args| contains the histogram name, and
  // the value to record.
  void HandleRecordBooleanHistogram(const base::ListValue* args);

  // Records a millisecond time value in a histogram, similar to
  // UMA_HISTOGRAM_TIMES. Handles times between 1ms and 10sec. |args|
  // contains the histogram name and a value in milliseconds.
  void HandleRecordTime(const base::ListValue* args);

  // Records a millisecond time value in a histogram, similar to
  // UmaHistogramMedium. Handles times up to 3 minutes. |args| contains the
  // histogram name and a value in milliseconds.
  void HandleRecordMediumTime(const base::ListValue* args);

 private:
  DISALLOW_COPY_AND_ASSIGN(MetricsHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_METRICS_HANDLER_H_
