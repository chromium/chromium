// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/annotation_reducer/ingested_content_annotation.h"

namespace accessibility_annotator {

IngestedContentAnnotation::IngestedContentAnnotation() = default;

IngestedContentAnnotation::IngestedContentAnnotation(
    std::string id,
    GURL url,
    base::Time timestamp,
    std::string description,
    AnnotationStatus status,
    std::vector<Entity> structured_entities,
    std::vector<optimization_guide::proto::DynamicAttribute> supplemental_data)
    : id(std::move(id)),
      url(std::move(url)),
      timestamp(timestamp),
      description(std::move(description)),
      status(status),
      structured_entities(std::move(structured_entities)),
      supplemental_data(std::move(supplemental_data)) {}

IngestedContentAnnotation::IngestedContentAnnotation(
    const IngestedContentAnnotation&) = default;
IngestedContentAnnotation& IngestedContentAnnotation::operator=(
    const IngestedContentAnnotation&) = default;
IngestedContentAnnotation::IngestedContentAnnotation(
    IngestedContentAnnotation&&) = default;
IngestedContentAnnotation& IngestedContentAnnotation::operator=(
    IngestedContentAnnotation&&) = default;
IngestedContentAnnotation::~IngestedContentAnnotation() = default;

}  // namespace accessibility_annotator
