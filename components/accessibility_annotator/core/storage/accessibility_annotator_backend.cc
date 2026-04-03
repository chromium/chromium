// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/storage/accessibility_annotator_backend.h"

namespace accessibility_annotator {

AccessibilityAnnotatorBackend::ContentAnnotationsData::
    ContentAnnotationsData() = default;

AccessibilityAnnotatorBackend::ContentAnnotationsData::
    ~ContentAnnotationsData() = default;

AccessibilityAnnotatorBackend::ContentAnnotationsData::ContentAnnotationsData(
    ContentAnnotationsData&& other) = default;

AccessibilityAnnotatorBackend::ContentAnnotationsData&
AccessibilityAnnotatorBackend::ContentAnnotationsData::operator=(
    ContentAnnotationsData&& other) = default;

}  // namespace accessibility_annotator
