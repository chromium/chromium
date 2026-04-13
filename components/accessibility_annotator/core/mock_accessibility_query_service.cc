// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/mock_accessibility_query_service.h"

#include <memory>

#include "components/accessibility_annotator/core/accessibility_query_service_delegate.h"
#include "components/accessibility_annotator/core/annotation_reducer/memory_data_provider.h"

namespace accessibility_annotator {

namespace {

class StubAccessibilityQueryServiceDelegate
    : public accessibility_annotator::AccessibilityQueryServiceDelegate {
 public:
  void RetrieveLiveTabContext(
      accessibility_annotator::LiveTabContextQuery query,
      base::OnceCallback<void(accessibility_annotator::LiveTabContextResponse)>
          callback) override {
    std::move(callback).Run({});
  }
};

}  // namespace

MockAccessibilityQueryService::MockAccessibilityQueryService()
    : accessibility_annotator::AccessibilityQueryService(
          std::make_unique<StubAccessibilityQueryServiceDelegate>(),
          /*data_providers=*/{},
          /*one_p_resolver=*/nullptr,
          /*remote_model_executor=*/nullptr) {}

MockAccessibilityQueryService::~MockAccessibilityQueryService() = default;

}  // namespace accessibility_annotator
