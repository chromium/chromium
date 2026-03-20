// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/mock_accessibility_query_service.h"

#include "components/accessibility_annotator/core/annotation_reducer/autofill_data_provider.h"

namespace autofill {

MockAccessibilityQueryService::MockAccessibilityQueryService()
    : accessibility_annotator::AccessibilityQueryService(
          /*data_provider=*/nullptr) {}

MockAccessibilityQueryService::~MockAccessibilityQueryService() = default;

}  // namespace autofill
