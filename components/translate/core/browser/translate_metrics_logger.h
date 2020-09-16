// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_METRICS_LOGGER_H_
#define COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_METRICS_LOGGER_H_

namespace translate {

// TranslateMetricsLogger tracks and logs various UKM and UMA metrics for Chrome
// Translate over the course of a page load.
class TranslateMetricsLogger {
 public:
  TranslateMetricsLogger() = default;
  virtual ~TranslateMetricsLogger() = default;

  TranslateMetricsLogger(const TranslateMetricsLogger&) = delete;
  TranslateMetricsLogger& operator=(const TranslateMetricsLogger&) = delete;

  // Tracks the state of the page over the course of a page load.
  virtual void OnPageLoadStart(bool is_foreground) = 0;
  virtual void OnForegroundChange(bool is_foreground) = 0;

  // Logs all stored page load metrics. If is_final is |true| then RecordMetrics
  // won't be called again.
  virtual void RecordMetrics(bool is_final) = 0;
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_METRICS_LOGGER_H_
