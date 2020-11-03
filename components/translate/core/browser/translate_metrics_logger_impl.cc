// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_metrics_logger_impl.h"

#include "base/metrics/histogram_functions.h"
#include "components/translate/core/browser/translate_manager.h"

namespace translate {

const char kTranslatePageLoadAutofillAssistantDeferredTriggerDecision[] =
    "Translate.PageLoad.AutofillAssistantDeferredTriggerDecision";
const char kTranslatePageLoadRankerDecision[] =
    "Translate.PageLoad.Ranker.Decision";
const char kTranslatePageLoadRankerVersion[] =
    "Translate.PageLoad.Ranker.Version";
const char kTranslatePageLoadTriggerDecision[] =
    "Translate.PageLoad.TriggerDecision";

TranslateMetricsLoggerImpl::TranslateMetricsLoggerImpl(
    base::WeakPtr<TranslateManager> translate_manager)
    : translate_manager_(translate_manager) {}

TranslateMetricsLoggerImpl::~TranslateMetricsLoggerImpl() = default;

void TranslateMetricsLoggerImpl::OnPageLoadStart(bool is_foreground) {
  if (translate_manager_)
    translate_manager_->RegisterTranslateMetricsLogger(
        weak_method_factory_.GetWeakPtr());

  is_foreground_ = is_foreground;
}

void TranslateMetricsLoggerImpl::OnForegroundChange(bool is_foreground) {
  is_foreground_ = is_foreground;
}

void TranslateMetricsLoggerImpl::RecordMetrics(bool is_final) {
  // The first time |RecordMetrics| is called, record all page load frequency
  // UMA metrcis.
  if (sequence_no_ == 0)
    RecordPageLoadUmaMetrics();

  // TODO(curranmax): Log UKM metrics now that the page load is.
  // completed. https://crbug.com/1114868.

  sequence_no_++;
}

void TranslateMetricsLoggerImpl::RecordPageLoadUmaMetrics() {
  base::UmaHistogramEnumeration(kTranslatePageLoadRankerDecision,
                                ranker_decision_);
  base::UmaHistogramSparse(kTranslatePageLoadRankerVersion,
                           int(ranker_version_));
  base::UmaHistogramEnumeration(kTranslatePageLoadTriggerDecision,
                                trigger_decision_);
  base::UmaHistogramBoolean(
      kTranslatePageLoadAutofillAssistantDeferredTriggerDecision,
      autofill_assistant_deferred_trigger_decision_);
}

void TranslateMetricsLoggerImpl::LogRankerMetrics(
    RankerDecision ranker_decision,
    uint32_t ranker_version) {
  ranker_decision_ = ranker_decision;
  ranker_version_ = ranker_version;
}

void TranslateMetricsLoggerImpl::LogTriggerDecision(
    TriggerDecision trigger_decision) {
  // Only stores the first non-kUninitialized trigger decision in the event that
  // there are multiple.
  if (trigger_decision_ == TriggerDecision::kUninitialized)
    trigger_decision_ = trigger_decision;
}

void TranslateMetricsLoggerImpl::LogAutofillAssistantDeferredTriggerDecision() {
  autofill_assistant_deferred_trigger_decision_ = true;
}

}  // namespace translate
