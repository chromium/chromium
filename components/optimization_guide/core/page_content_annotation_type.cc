// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/page_content_annotation_type.h"

namespace optimization_guide {

// Each of these string values is used in UMA histograms so please update the
// variants there when any changes are made.
// //tools/metrics/histograms/metadata/optimization/histograms.xml
std::string AnnotationTypeToString(AnnotationType type) {
  switch (type) {
    case AnnotationType::kUnknown:
      return "Unknown";
    case AnnotationType::kContentVisibility:
      return "ContentVisibility";
    case AnnotationType::kPageEntities:
      return "PageEntities";
  }
}

}  // namespace optimization_guide