// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/accessibility_annotator_service.h"

#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/callback.h"

namespace accessibility_annotator {

AccessibilityAnnotatorService::AccessibilityAnnotatorService(
    std::unique_ptr<EntityDataProvider> entity_data_provider)
    : entity_data_provider_(std::move(entity_data_provider)) {}

AccessibilityAnnotatorService::~AccessibilityAnnotatorService() = default;

EntityDataProvider* AccessibilityAnnotatorService::GetEntityDataProvider() {
  return entity_data_provider_.get();
}

}  // namespace accessibility_annotator
