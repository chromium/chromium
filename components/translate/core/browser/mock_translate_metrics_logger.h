// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// // found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_BROWSER_MOCK_TRANSLATE_METRICS_LOGGER_H_
#define COMPONENTS_TRANSLATE_CORE_BROWSER_MOCK_TRANSLATE_METRICS_LOGGER_H_

#include "components/translate/core/browser/translate_metrics_logger.h"

#include "base/memory/weak_ptr.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace translate {

namespace testing {

class MockTranslateMetricsLogger : public TranslateMetricsLogger {
 public:
  MockTranslateMetricsLogger();
  ~MockTranslateMetricsLogger() override;

  MockTranslateMetricsLogger(const MockTranslateMetricsLogger&) = delete;
  MockTranslateMetricsLogger& operator=(const MockTranslateMetricsLogger&) =
      delete;

  base::WeakPtr<MockTranslateMetricsLogger> GetWeakPtr();

  MOCK_METHOD1(OnPageLoadStart, void(bool));
  MOCK_METHOD1(OnForegroundChange, void(bool));
  MOCK_METHOD1(RecordMetrics, void(bool));
  MOCK_METHOD2(LogRankerMetrics, void(RankerDecision, uint32_t));
  MOCK_METHOD1(LogTriggerDecision, void(TriggerDecision));
  MOCK_METHOD0(LogAutofillAssistantDeferredTriggerDecision, void());
  MOCK_METHOD0(LogInitialState, void());
  MOCK_METHOD0(LogTranslationStarted, void());
  MOCK_METHOD1(LogTranslationFinished, void(bool));
  MOCK_METHOD0(LogReversion, void());
  MOCK_METHOD1(LogUIChange, void(bool));
  MOCK_METHOD1(LogOmniboxIconChange, void(bool));

 private:
  base::WeakPtrFactory<MockTranslateMetricsLogger> weak_method_factory_{this};
};

}  // namespace testing

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CORE_BROWSER_MOCK_TRANSLATE_METRICS_LOGGER_H_
