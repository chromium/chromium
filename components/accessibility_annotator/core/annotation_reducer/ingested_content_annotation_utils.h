// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ANNOTATION_REDUCER_INGESTED_CONTENT_ANNOTATION_UTILS_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ANNOTATION_REDUCER_INGESTED_CONTENT_ANNOTATION_UTILS_H_

#include "components/accessibility_annotator/core/annotation_reducer/ingested_content_annotation.h"
#include "components/accessibility_annotator/core/storage/accessibility_annotator_backend.h"

namespace accessibility_annotator {

// Converts a `ContentAnnotationData` to an `IngestedContentAnnotation`.
IngestedContentAnnotation ConvertIngestedContentAnnotation(
    const AccessibilityAnnotatorBackend::ContentAnnotationsData&
        content_annotation_data);

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ANNOTATION_REDUCER_INGESTED_CONTENT_ANNOTATION_UTILS_H_
