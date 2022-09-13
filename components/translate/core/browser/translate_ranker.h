// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_RANKER_H_
#define COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_RANKER_H_

#include <memory>
#include <vector>

#include "components/keyed_service/core/keyed_service.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace metrics {
class TranslateEventProto;
}  // namespace metrics

namespace translate {

class TranslateMetricsLogger;

// If enabled, downloads a translate ranker model and uses it to determine
// whether the user should be given a translation prompt or not.
class TranslateRanker : public KeyedService {
 public:
  TranslateRanker() = default;

  TranslateRanker(const TranslateRanker&) = delete;
  TranslateRanker& operator=(const TranslateRanker&) = delete;

  // Returns the version id for the ranker model.
  virtual uint32_t GetModelVersion() const = 0;

  // Returns true if executing the ranker model in the translation prompt
  // context described by |translate_event| and possibly
  // other global browser context attributes suggests that the user should be
  // prompted as to whether translation should be performed.
  virtual bool ShouldOfferTranslation(
      metrics::TranslateEventProto* translate_event,
      TranslateMetricsLogger* translate_metrics_logger) = 0;

  // Transfers cached translate events to the given vector pointer and clears
  // the cache.
  virtual void FlushTranslateEvents(
      std::vector<metrics::TranslateEventProto>* events) = 0;

  // Record |translate_event| with the given |event_type| and |url|.
  // |event_type| must be one of the values defined by
  // metrics::TranslateEventProto::EventType.
  virtual void RecordTranslateEvent(
      int event_type,
      ukm::SourceId ukm_source_id,
      metrics::TranslateEventProto* translate_event) = 0;

  // If override of MATCHES_PREVIOUS_LANGUAGE is enabled, will return true and
  // add MATCHES_PREVIOUS_LANGUAGE to |translate_event.decision_overrides()|. If
  // override is disabled, returns false and finalizes and records
  // |translate_event| with MATCHES_PREVIOUS_LANGUAGE event type.
  virtual bool ShouldOverrideMatchesPreviousLanguageDecision(
      ukm::SourceId ukm_source_id,
      metrics::TranslateEventProto* translate_event) = 0;

  // Override the default enabled/disabled state of translate event logging.
  virtual void EnableLogging(bool enable_logging) = 0;
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_RANKER_H_
