// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/accessibility_annotator_enablement_service_impl.h"

#include <string>

#include "base/feature_list.h"
#include "base/strings/strcat.h"
#include "components/accessibility_annotator/core/accessibility_annotator_features.h"

namespace accessibility_annotator {
namespace {
// Helper function for debugging why a permissions check failed.
void MaybeOutputReason(std::string* out, std::string_view message) {
  if (out) {
    *out = std::string(message);
  }
}

// Checks whether all requirements for `base::Feature` state are satisfied.
[[nodiscard]] bool SatisfiesFeatureRequirements(
    std::string* debug_message = nullptr) {
  const base::Feature* const kRequiredFeatures[] = {
      &features::kAccessibilityAnnotator,
      &features::kAccessibilityAnnotatorFirstRun,
      &features::kAccessibilityAnnotatorDatabaseStorage,
  };

  for (const base::Feature* feature : kRequiredFeatures) {
    if (!base::FeatureList::IsEnabled(*feature)) {
      MaybeOutputReason(debug_message,
                        base::StrCat({feature->name, " is not enabled."}));
      return false;
    }
  }

  return true;
}
}  // namespace

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
  using enum RemoteAnnotatorEnablementState;

  if (!SatisfiesFeatureRequirements()) {
    return kDisabledNotEligible;
  }
  // TODO(b/497763332): Implement the real enablement state logic.
  return kDisabledPendingInfo;
}

}  // namespace accessibility_annotator
