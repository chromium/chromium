// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/mock_translate_ranker.h"

#include "third_party/metrics_proto/translate_event.pb.h"
#include "url/gurl.h"

namespace translate {
namespace testing {

MockTranslateRanker::MockTranslateRanker() {}

MockTranslateRanker::~MockTranslateRanker() {}

uint32_t MockTranslateRanker::GetModelVersion() const {
  return model_version_;
}

bool MockTranslateRanker::ShouldOfferTranslation(
    metrics::TranslateEventProto* /*translate_event */,
    TranslateMetricsLogger* /*translate_metrics_logger*/) {
  return should_offer_translation_;
}

void MockTranslateRanker::FlushTranslateEvents(
    std::vector<metrics::TranslateEventProto>* events) {
  events->swap(event_cache_);
  event_cache_.clear();
}

}  // namespace testing
}  // namespace translate
