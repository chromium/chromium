// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_ANNOTATOR_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_ANNOTATOR_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "components/optimization_guide/core/page_content_annotations_common.h"

namespace optimization_guide {

using BatchAnnotationCallback =
    base::OnceCallback<void(const std::vector<BatchAnnotationResult>&)>;

// A virtual interface that is called to run a page content annotation. This
// interface can be implemented by testing mocks.
class PageContentAnnotator {
 public:
  virtual ~PageContentAnnotator() = default;

  // Annotates all |inputs| according to the |annotation_type| and returns the
  // result to the given |callback|. The vector size passed to the callback will
  // always match the size and ordering of |inputs|.
  virtual void Annotate(BatchAnnotationCallback callback,
                        const std::vector<std::string>& inputs,
                        AnnotationType annotation_type) = 0;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CONTENT_BROWSER_PAGE_CONTENT_ANNOTATOR_H_
