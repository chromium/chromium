// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/accessibility_query_service.h"

#include <memory>
#include <utility>

#include "components/accessibility_annotator/core/annotation_reducer/autofill_data_provider.h"
#include "components/accessibility_annotator/core/annotation_reducer/query_classifier.h"
#include "components/accessibility_annotator/core/annotation_reducer/query_intent_type.h"

namespace accessibility_annotator {

AccessibilityQueryService::AccessibilityQueryService(
    std::unique_ptr<AutofillDataProvider> data_provider)
    : data_provider_(std::move(data_provider)),
      classifier_(CreateQueryClassifier()) {}

AccessibilityQueryService::~AccessibilityQueryService() = default;

void AccessibilityQueryService::Shutdown() {
  data_provider_.reset();
}

std::vector<MemorySearchResult> AccessibilityQueryService::Query(
    std::u16string_view query) {
  if (!data_provider_) {
    return {};
  }

  QueryIntentType intent = classifier_.Run(query);
  if (intent == QueryIntentType::kUnknown) {
    return {};
  }

  return data_provider_->RetrieveAll(intent);
}

}  // namespace accessibility_annotator
