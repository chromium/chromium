// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_BROWSER_MOCK_TRANSLATE_RANKER_H_
#define COMPONENTS_TRANSLATE_CORE_BROWSER_MOCK_TRANSLATE_RANKER_H_

#include <memory>
#include <string>
#include <vector>

#include "components/translate/core/browser/translate_ranker.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gmock/include/gmock/gmock.h"


namespace metrics {
class TranslateEventProto;
}

namespace translate {

namespace testing {

class MockTranslateRanker : public TranslateRanker {
 public:
  MockTranslateRanker();
  ~MockTranslateRanker() override;

  void set_is_logging_enabled(bool val) { is_logging_enabled_ = val; }
  void set_is_query_enabled(bool val) { is_query_enabled_ = val; }
  void set_is_enforcement_enabled(bool val) { is_enforcement_enabled_ = val; }
  void set_is_decision_override_enabled(bool val) {
    is_decision_override_enabled_ = val;
  }
  void set_model_version(int val) { model_version_ = val; }
  void set_should_offer_translation(bool val) {
    should_offer_translation_ = val;
  }

  // TranslateRanker Implementation:
  uint32_t GetModelVersion() const override;
  void EnableLogging(bool logging_enabled) override {
    is_logging_enabled_ = logging_enabled;
  }
  bool ShouldOfferTranslation(
      metrics::TranslateEventProto* translate_events) override;
  void FlushTranslateEvents(
      std::vector<metrics::TranslateEventProto>* events) override;
  MOCK_METHOD3(RecordTranslateEvent,
               void(int event_type,
                    ukm::SourceId ukm_source_id,
                    metrics::TranslateEventProto* translate_event));
  MOCK_METHOD3(ShouldOverrideDecision,
               bool(int event_type,
                    ukm::SourceId ukm_source_id,
                    metrics::TranslateEventProto* translate_event));

 private:
  std::vector<metrics::TranslateEventProto> event_cache_;

  bool is_logging_enabled_ = false;
  bool is_query_enabled_ = false;
  bool is_enforcement_enabled_ = false;
  bool is_decision_override_enabled_ = false;
  bool model_version_ = 0;
  bool should_offer_translation_ = true;

  DISALLOW_COPY_AND_ASSIGN(MockTranslateRanker);
};

}  // namespace testing

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CORE_BROWSER_MOCK_TRANSLATE_RANKER_H_
