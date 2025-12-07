// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_PAGE_VISIBILITY_OP_RESOLVER_H_
#define COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_PAGE_VISIBILITY_OP_RESOLVER_H_

#include "components/optimization_guide/core/tflite_op_resolver.h"

namespace page_content_annotations {

class PageVisibilityOpResolver : public optimization_guide::TFLiteOpResolver {
 public:
  PageVisibilityOpResolver();
  ~PageVisibilityOpResolver() override;
};

}  // namespace page_content_annotations

#endif  // COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_PAGE_VISIBILITY_OP_RESOLVER_H_
