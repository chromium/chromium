// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_METRICS_LOGGER_IMPL_H_
#define COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_METRICS_LOGGER_IMPL_H_

#include <memory>

#include "components/translate/core/browser/translate_metrics_logger.h"

namespace translate {

// TranslateMetricsLogger tracks and logs various UKM and UMA metrics for Chrome
// Translate over the course of a page load.
class TranslateMetricsLoggerImpl : public TranslateMetricsLogger {
 public:
  TranslateMetricsLoggerImpl() = default;
  ~TranslateMetricsLoggerImpl() override = default;

  TranslateMetricsLoggerImpl(const TranslateMetricsLoggerImpl&) = delete;
  TranslateMetricsLoggerImpl& operator=(const TranslateMetricsLoggerImpl&) =
      delete;

  // TranslateMetricsLogger
  void OnPageLoadStart(bool is_foreground) override;
  void OnForegroundChange(bool is_foreground) override;
  void RecordMetrics(bool is_final) override;

  // TODO(curranmax): Connect to TranslateManager so metrics can be collected
  // from the rest of the Translate code. https://crbug.com/1114868.
  // TODO(curranmax): Add appropriate functions for the Translate code to log
  // relevant events. https://crbug.com/1114868.
 private:
  // Since |RecordMetrics()| can be called multiple times, such as when Chrome
  // is backgrounded and reopened, we use |sequence_no_| to differentiate the
  // recorded UKM protos.
  unsigned int sequence_no_{0};

  // Tracks if the associated page is in the foreground (|true|) or the
  // background (|false|)
  bool is_foreground_{false};
};

}  // namespace translate

// TODO(curranmax): Add unit tests for this class. https://crbug.com/1114868.

#endif  // COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_METRICS_LOGGER_IMPL_H_
