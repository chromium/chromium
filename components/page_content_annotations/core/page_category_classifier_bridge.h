// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_PAGE_CATEGORY_CLASSIFIER_BRIDGE_H_
#define COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_PAGE_CATEGORY_CLASSIFIER_BRIDGE_H_

namespace page_content_annotations {

// An interface for a bridge that connects the on-device category classifier to
// a page embeddings source.
class PageCategoryClassifierBridge {
 public:
  virtual ~PageCategoryClassifierBridge() = default;
};

}  // namespace page_content_annotations

#endif  // COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_PAGE_CATEGORY_CLASSIFIER_BRIDGE_H_
