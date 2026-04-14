// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ACCESSIBILITY_ANNOTATOR_ENABLEMENT_SERVICE_IMPL_TEST_API_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ACCESSIBILITY_ANNOTATOR_ENABLEMENT_SERVICE_IMPL_TEST_API_H_

#include "base/check_deref.h"
#include "base/memory/raw_ref.h"
#include "components/accessibility_annotator/core/accessibility_annotator_enablement_service_impl.h"

namespace accessibility_annotator {

class AccessibilityAnnotatorEnablementServiceImplTestApi {
 public:
  explicit AccessibilityAnnotatorEnablementServiceImplTestApi(
      AccessibilityAnnotatorEnablementServiceImpl* service)
      : service_(CHECK_DEREF(service)) {}

  // Refreshes the enablement state. It should only ever be called in tests
  // after features got changed.
  void RecomputeEnablementState() { service_->UpdateEnablementState(); }

 private:
  const raw_ref<AccessibilityAnnotatorEnablementServiceImpl> service_;
};

inline AccessibilityAnnotatorEnablementServiceImplTestApi test_api(
    AccessibilityAnnotatorEnablementServiceImpl* service) {
  return AccessibilityAnnotatorEnablementServiceImplTestApi(service);
}

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ACCESSIBILITY_ANNOTATOR_ENABLEMENT_SERVICE_IMPL_TEST_API_H_
