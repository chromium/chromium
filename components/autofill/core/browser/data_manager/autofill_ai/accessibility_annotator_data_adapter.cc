// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_manager/autofill_ai/accessibility_annotator_data_adapter.h"

namespace autofill {

AccessibilityAnnotatorDataAdapter::AccessibilityAnnotatorDataAdapter() =
    default;

AccessibilityAnnotatorDataAdapter::~AccessibilityAnnotatorDataAdapter() =
    default;

std::vector<EntityInstance>
AccessibilityAnnotatorDataAdapter::GetEntityInstances() {
  // TODO(crbug.com/484123834): Implement.
  return {};
}

void AccessibilityAnnotatorDataAdapter::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void AccessibilityAnnotatorDataAdapter::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace autofill
