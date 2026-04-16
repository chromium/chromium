// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ANNOTATION_REDUCER_INGESTED_CONTENT_ANNOTATION_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ANNOTATION_REDUCER_INGESTED_CONTENT_ANNOTATION_H_

#include <string>
#include <vector>

#include "base/time/time.h"
#include "components/accessibility_annotator/core/data_models/entity.h"
#include "components/accessibility_annotator/core/data_models/entity_types.h"
#include "components/optimization_guide/proto/features/content_annotation.pb.h"
#include "url/gurl.h"

namespace accessibility_annotator {

// Represents an annotation that has been ingested from the backend and is
// ready to be processed by the annotation reducer and stored in the
// ContentAnnotationStore.
struct IngestedContentAnnotation {
  // Representation of ContentAnnotation_Status from the backend proto.
  enum class AnnotationStatus { kUnknown, kConfirmed, kPending };

  IngestedContentAnnotation();
  IngestedContentAnnotation(
      std::string id,
      GURL url,
      base::Time timestamp,
      std::string description,
      AnnotationStatus status,
      std::vector<Entity> structured_entities,
      std::vector<optimization_guide::proto::DynamicAttribute>
          supplemental_data);
  IngestedContentAnnotation(const IngestedContentAnnotation&);
  IngestedContentAnnotation& operator=(const IngestedContentAnnotation&);
  IngestedContentAnnotation(IngestedContentAnnotation&&);
  IngestedContentAnnotation& operator=(IngestedContentAnnotation&&);
  ~IngestedContentAnnotation();

  std::string id;
  GURL url;
  base::Time timestamp;
  std::string description;
  AnnotationStatus status = AnnotationStatus::kUnknown;
  std::vector<Entity> structured_entities;
  std::vector<optimization_guide::proto::DynamicAttribute> supplemental_data;
};

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_ANNOTATION_REDUCER_INGESTED_CONTENT_ANNOTATION_H_
