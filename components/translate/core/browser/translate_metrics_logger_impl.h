// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_METRICS_LOGGER_IMPL_H_
#define COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_METRICS_LOGGER_IMPL_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "components/translate/core/browser/translate_metrics_logger.h"

namespace translate {

class NullTranslateMetricsLogger : public TranslateMetricsLogger {
 public:
  NullTranslateMetricsLogger() = default;

  // TranslateMetricsLogger
  void OnPageLoadStart(bool is_foreground) override {}
  void OnForegroundChange(bool is_foreground) override {}
  void RecordMetrics(bool is_final) override {}
};

class TranslateManager;

// TranslateMetricsLogger tracks and logs various UKM and UMA metrics for Chrome
// Translate over the course of a page load.
class TranslateMetricsLoggerImpl : public TranslateMetricsLogger {
 public:
  explicit TranslateMetricsLoggerImpl(
      base::WeakPtr<TranslateManager> translate_manager);
  ~TranslateMetricsLoggerImpl() override;

  TranslateMetricsLoggerImpl(const TranslateMetricsLoggerImpl&) = delete;
  TranslateMetricsLoggerImpl& operator=(const TranslateMetricsLoggerImpl&) =
      delete;

  // TranslateMetricsLogger
  void OnPageLoadStart(bool is_foreground) override;
  void OnForegroundChange(bool is_foreground) override;
  void RecordMetrics(bool is_final) override;

  // TODO(curranmax): Add appropriate functions for the Translate code to log
  // relevant events. https://crbug.com/1114868.
 private:
  base::WeakPtr<TranslateManager> translate_manager_;

  // Since |RecordMetrics()| can be called multiple times, such as when Chrome
  // is backgrounded and reopened, we use |sequence_no_| to differentiate the
  // recorded UKM protos.
  unsigned int sequence_no_{0};

  // Tracks if the associated page is in the foreground (|true|) or the
  // background (|false|)
  bool is_foreground_{false};

  base::WeakPtrFactory<TranslateMetricsLoggerImpl> weak_method_factory_{this};
};

}  // namespace translate

// TODO(curranmax): Add unit tests for this class. https://crbug.com/1114868.

#endif  // COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_METRICS_LOGGER_IMPL_H_
