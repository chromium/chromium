// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_PAGE_CONTENT_ANNOTATION_TYPE_H_
#define COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_PAGE_CONTENT_ANNOTATION_TYPE_H_

#include <string>

namespace page_content_annotations {

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
  //
  // This is deprecated and should not be used.
  kDeprecatedPageEntities,

  // The input will be annotated for text embedding.
  //
  // This is deprecated and should not be used.
  kDeprecatedTextEmbedding,
};

std::string AnnotationTypeToString(AnnotationType type);

}  // namespace page_content_annotations

#endif  // COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_PAGE_CONTENT_ANNOTATION_TYPE_H_
