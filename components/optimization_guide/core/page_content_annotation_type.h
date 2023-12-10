// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_PAGE_CONTENT_ANNOTATION_TYPE_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_PAGE_CONTENT_ANNOTATION_TYPE_H_

#include <string>

#include "base/component_export.h"

namespace optimization_guide {

// The type of annotation that is being done on the given input.
//
// Each of these is used in UMA histograms so please update the variants there
// when any changes are made.
// //tools/metrics/histograms/metadata/optimization/histograms.xml
enum class AnnotationType {
  kUnknown,

  // The input will be annotated for the visibility of the content.
  kContentVisibility,

  // The input will be annotated with the entities on the page. If the entities
  // will be persisted, make sure that only the entity IDs are persisted. To map
  // the IDs back to human-readable strings, use `EntityMetadataProvider`.
  kPageEntities,

  // The input will be annotated for text embedding.
  kTextEmbedding,
};

COMPONENT_EXPORT(OPTIMIZATION_GUIDE_FEATURES)
std::string AnnotationTypeToString(AnnotationType type);

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_PAGE_CONTENT_ANNOTATION_TYPE_H_
