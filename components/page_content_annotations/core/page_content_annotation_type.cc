// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/core/page_content_annotation_type.h"

#include "base/notreached.h"

namespace page_content_annotations {

// Each of these string values is used in UMA histograms so please update the
// variants there when any changes are made.
// //tools/metrics/histograms/metadata/optimization/histograms.xml
std::string AnnotationTypeToString(AnnotationType type) {
  switch (type) {
    case AnnotationType::kUnknown:
      return "Unknown";
    case AnnotationType::kContentVisibility:
      return "ContentVisibility";
    case AnnotationType::kDeprecatedPageEntities:
    case AnnotationType::kDeprecatedTextEmbedding:
      NOTREACHED_IN_MIGRATION();
      return "";
  }
}

}  // namespace page_content_annotations
