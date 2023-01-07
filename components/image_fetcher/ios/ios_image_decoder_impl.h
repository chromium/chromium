// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IMAGE_FETCHER_IOS_IOS_IMAGE_DECODER_IMPL_H_
#define COMPONENTS_IMAGE_FETCHER_IOS_IOS_IMAGE_DECODER_IMPL_H_

#include <memory>

#include "components/image_fetcher/core/image_decoder.h"

namespace image_fetcher {

// Factory for iOS specific implementation of ImageDecoder.
std::unique_ptr<ImageDecoder> CreateIOSImageDecoder();

}  // namespace image_fetcher

#endif  // COMPONENTS_IMAGE_FETCHER_IOS_IOS_IMAGE_DECODER_IMPL_H_
