// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_PAGE_CONTENT_ANNOTATIONS_ENUMS_H_
#define COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_PAGE_CONTENT_ANNOTATIONS_ENUMS_H_

namespace page_content_annotations {

// The status for the page content annotations being stored.
//
// Keep in sync with PageContentAnnotationsStorageStatus in enums.xml.
enum PageContentAnnotationsStorageStatus {
  kUnknown = 0,
  // The content annotations were requested to be stored in the History Service.
  kSuccess = 1,
  // There were no visits for the URL found in the History Service.
  kNoVisitsForUrl = 2,
  // The specific visit that we wanted to annotate could not be found in the
  // History Service.
  kSpecificVisitForUrlNotFound = 3,

  // Add new values above this line.
  kMaxValue = kSpecificVisitForUrlNotFound,
};

}  // namespace page_content_annotations

#endif  // COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_PAGE_CONTENT_ANNOTATIONS_ENUMS_H_
