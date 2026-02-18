// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_PAGE_CONTENT_EXTRACTION_TYPES_H_
#define COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_PAGE_CONTENT_EXTRACTION_TYPES_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"

namespace page_content_annotations {

using RefCountedAnnotatedPageContent =
    base::RefCountedData<optimization_guide::proto::AnnotatedPageContent>;

struct ExtractedPageContentResult {
  ExtractedPageContentResult();
  ~ExtractedPageContentResult();

  ExtractedPageContentResult(
      scoped_refptr<const RefCountedAnnotatedPageContent> page_content,
      base::Time extraction_timestamp,
      bool is_eligible_for_server_upload,
      std::vector<uint8_t> screenshot_data);

  ExtractedPageContentResult(const ExtractedPageContentResult&);
  ExtractedPageContentResult& operator=(const ExtractedPageContentResult&);
  ExtractedPageContentResult(ExtractedPageContentResult&&);
  ExtractedPageContentResult& operator=(ExtractedPageContentResult&&);

  // The AnnotatedPageContent proto that represents the page content.
  scoped_refptr<const RefCountedAnnotatedPageContent> page_content;

  // The timestamp when the extraction is triggered.
  base::Time extraction_timestamp;

  // Whether the content is eligible for server upload.
  bool is_eligible_for_server_upload = false;

  std::vector<uint8_t> screenshot_data;
};

}  // namespace page_content_annotations

#endif  // COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_PAGE_CONTENT_EXTRACTION_TYPES_H_
