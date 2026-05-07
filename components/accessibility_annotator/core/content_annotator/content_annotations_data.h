// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_CONTENT_ANNOTATOR_CONTENT_ANNOTATIONS_DATA_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_CONTENT_ANNOTATOR_CONTENT_ANNOTATIONS_DATA_H_

#include <optional>
#include <string>

#include "base/time/time.h"
#include "base/values.h"
#include "components/history/core/browser/history_types.h"
#include "components/optimization_guide/proto/features/content_annotation.pb.h"
#include "url/gurl.h"

namespace accessibility_annotator {

struct ContentAnnotationsData {
  ContentAnnotationsData();
  ContentAnnotationsData(ContentAnnotationsData&& other);
  ContentAnnotationsData& operator=(ContentAnnotationsData&& other);

  ContentAnnotationsData(const ContentAnnotationsData&) = delete;
  ContentAnnotationsData& operator=(const ContentAnnotationsData&) = delete;

  ~ContentAnnotationsData();

  ContentAnnotationsData Clone() const;

  // LINT.IfChange
  std::string page_title;
  std::optional<int> tab_id;
  optimization_guide::proto::ContentAnnotation content_annotation;
  base::DictValue classifier_results;
  base::Time navigation_timestamp;
  GURL url;
  // LINT.ThenChange(//components/accessibility_annotator/core/content_annotator/content_annotations_data.cc:ContentAnnotationsDataClone)
};

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_CONTENT_ANNOTATOR_CONTENT_ANNOTATIONS_DATA_H_
