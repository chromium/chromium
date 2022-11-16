// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_PAGE_VISIBILITY_OP_RESOLVER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_PAGE_VISIBILITY_OP_RESOLVER_H_

#include "components/optimization_guide/core/tflite_op_resolver.h"

namespace optimization_guide {

class PageVisibilityOpResolver : public TFLiteOpResolver {
 public:
  PageVisibilityOpResolver();
  ~PageVisibilityOpResolver() override;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_PAGE_VISIBILITY_OP_RESOLVER_H_
