// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_PAGE_CATEGORY_CLASSIFIER_BRIDGE_H_
#define COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_PAGE_CATEGORY_CLASSIFIER_BRIDGE_H_

#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "components/page_content_annotations/core/page_content_annotation_type.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

class GURL;

namespace page_content_annotations {

// An interface for a bridge that connects the on-device category classifier to
// a page embeddings source and delivers classified categories to subscribers.
class PageCategoryClassifierBridge {
 public:
  using ClassificationCallback =
      base::RepeatingCallback<void(const GURL& url,
                                   ukm::SourceId source_id,
                                   const std::vector<Category>& categories)>;

  virtual ~PageCategoryClassifierBridge() = default;

  // Subscribes to page category classifications. Active subscriptions also
  // express downstream demand to the pipeline.
  virtual base::CallbackListSubscription Subscribe(
      ClassificationCallback callback) = 0;
};

}  // namespace page_content_annotations

#endif  // COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_PAGE_CATEGORY_CLASSIFIER_BRIDGE_H_
