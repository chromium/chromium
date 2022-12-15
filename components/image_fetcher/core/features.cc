// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_fetcher/core/features.h"

namespace image_fetcher {
namespace features {

#if BUILDFLAG(IS_ANDROID)
// Enables batching decoding of related images in a single process.
BASE_FEATURE(kBatchImageDecoding,
             "BatchImageDecoding",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

}  // namespace features
}  // namespace image_fetcher
