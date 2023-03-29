// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_IMAGE_SERVICE_FEATURES_H_
#define COMPONENTS_PAGE_IMAGE_SERVICE_FEATURES_H_

#include "base/feature_list.h"

namespace page_image_service {

BASE_DECLARE_FEATURE(kImageService);
BASE_DECLARE_FEATURE(kImageServiceSuggestPoweredImages);
BASE_DECLARE_FEATURE(kImageServiceOptimizationGuideSalientImages);

}  // namespace page_image_service

#endif  // COMPONENTS_PAGE_IMAGE_SERVICE_FEATURES_H_
