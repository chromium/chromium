// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IMAGE_FETCHER_CORE_IMAGE_FETCHER_TYPES_H_
#define COMPONENTS_IMAGE_FETCHER_CORE_IMAGE_FETCHER_TYPES_H_

#include <string>

#include "base/functional/callback.h"

namespace gfx {
class Image;
}  // namespace gfx

#if defined(__OBJC__)
@class NSData;
#endif  // defined(__OBJC__)

namespace image_fetcher {

struct RequestMetadata;

using ImageFetcherCallback =
    base::OnceCallback<void(const gfx::Image& image,
                            const RequestMetadata& request_metadata)>;

// TODO(wylieb): Use RefCountedBytes to avoid copying.
using ImageDataFetcherCallback =
    base::OnceCallback<void(const std::string& image_data,
                            const RequestMetadata& request_metadata)>;

#if defined(__OBJC__)

// Callback that informs of the download of an image encoded in |data| and the
// associated metadata. If an error prevented a http response,
// |metadata.http_response_code| will be RESPONSE_CODE_INVALID.
using ImageDataFetcherBlock = void (^)(NSData* data,
                                       const RequestMetadata& metadata);

#endif  // defined(__OBJC__)

}  // namespace image_fetcher

#endif  // COMPONENTS_IMAGE_FETCHER_CORE_IMAGE_FETCHER_TYPES_H_
