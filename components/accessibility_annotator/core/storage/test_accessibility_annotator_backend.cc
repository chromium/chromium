// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/storage/test_accessibility_annotator_backend.h"

#include <utility>

#include "base/functional/callback.h"

namespace accessibility_annotator {

MockAccessibilityAnnotatorBackendObserver::
    MockAccessibilityAnnotatorBackendObserver() = default;

MockAccessibilityAnnotatorBackendObserver::
    ~MockAccessibilityAnnotatorBackendObserver() = default;

TestAccessibilityAnnotatorBackend::TestAccessibilityAnnotatorBackend() =
    default;

TestAccessibilityAnnotatorBackend::~TestAccessibilityAnnotatorBackend() =
    default;

}  // namespace accessibility_annotator
