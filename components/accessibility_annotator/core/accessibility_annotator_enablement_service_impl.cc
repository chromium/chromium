// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/accessibility_annotator_enablement_service_impl.h"

namespace accessibility_annotator {

AccessibilityAnnotatorEnablementServiceImpl::
    AccessibilityAnnotatorEnablementServiceImpl() = default;

AccessibilityAnnotatorEnablementServiceImpl::
    ~AccessibilityAnnotatorEnablementServiceImpl() = default;

void AccessibilityAnnotatorEnablementServiceImpl::AddObserver(
    Observer* observer) {
  observers_.AddObserver(observer);
}

void AccessibilityAnnotatorEnablementServiceImpl::RemoveObserver(
    Observer* observer) {
  observers_.RemoveObserver(observer);
}

RemoteAnnotatorEnablementState
AccessibilityAnnotatorEnablementServiceImpl::GetEnablementState() {
  // TODO(b/497763332): Implement the real enablement state logic.
  return RemoteAnnotatorEnablementState::kDisabledPendingInfo;
}

}  // namespace accessibility_annotator
