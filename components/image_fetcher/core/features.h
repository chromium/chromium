// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IMAGE_FETCHER_CORE_FEATURES_H_
#define COMPONENTS_IMAGE_FETCHER_CORE_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"

namespace image_fetcher {
namespace features {

#if BUILDFLAG(IS_ANDROID)
extern const base::Feature kBatchImageDecoding;
#endif

}  // namespace features
}  // namespace image_fetcher

#endif  // COMPONENTS_IMAGE_FETCHER_CORE_FEATURES_H_
