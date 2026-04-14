// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_BROWSER_MOCK_TRANSLATE_METRICS_LOGGER_H_
#define COMPONENTS_TRANSLATE_CORE_BROWSER_MOCK_TRANSLATE_METRICS_LOGGER_H_

#include <string>
#include <string_view>

#include "base/memory/weak_ptr.h"
#include "components/translate/core/browser/translate_metrics_logger.h"
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
  MOCK_METHOD1(SetUkmSourceId, void(ukm::SourceId));
  MOCK_METHOD2(LogRankerMetrics, void(RankerDecision, uint32_t));
  MOCK_METHOD0(LogRankerStart, void());
  MOCK_METHOD0(LogRankerFinish, void());
  MOCK_METHOD1(LogTriggerDecision, void(TriggerDecision));
  MOCK_METHOD0(LogInitialState, void());
  MOCK_METHOD1(LogHTMLContentLanguage, void(std::string_view));
  MOCK_METHOD1(LogHTMLDocumentLanguage, void(std::string_view));
  MOCK_METHOD1(LogDetectedLanguage, void(std::string_view));
  MOCK_METHOD1(LogDetectionReliabilityScore, void(const float&));
  MOCK_METHOD1(LogTranslationStarted, void(TranslationType));
  MOCK_METHOD2(LogTranslationFinished, void(bool, TranslateErrors));
  MOCK_METHOD0(LogReversion, void());
  MOCK_METHOD1(LogUIChange, void(bool));
  MOCK_METHOD1(LogOmniboxIconChange, void(bool));
  MOCK_METHOD2(LogInitialSourceLanguage, void(std::string_view, bool));
  MOCK_METHOD1(LogSourceLanguage, void(std::string_view));
  MOCK_METHOD2(LogTargetLanguage,
               void(std::string_view,
                    TranslateBrowserMetrics::TargetLanguageOrigin));
  MOCK_METHOD1(LogUIInteraction, void(UIInteraction));
  MOCK_METHOD1(GetNextManualTranslationType, TranslationType(bool));
  MOCK_METHOD1(SetHasHrefTranslateTarget, void(bool));
  MOCK_METHOD1(LogWasContentEmpty, void(bool));

 private:
  base::WeakPtrFactory<MockTranslateMetricsLogger> weak_method_factory_{this};
};

}  // namespace testing

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CORE_BROWSER_MOCK_TRANSLATE_METRICS_LOGGER_H_
