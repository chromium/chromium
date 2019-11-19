// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IMAGE_FETCHER_CORE_CACHE_IMAGE_STORE_TYPES_H_
#define COMPONENTS_IMAGE_FETCHER_CORE_CACHE_IMAGE_STORE_TYPES_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/optional.h"
#include "components/image_fetcher/core/cache/proto/cached_image_metadata.pb.h"

namespace image_fetcher {

// Represents the initialization status of a image storage module.
enum class InitializationStatus {
  UNINITIALIZED,
  INITIALIZED,
  INIT_FAILURE,
};

// Returns the resulting raw image data as a std::string. Data will be returned
// using move semantics. If |needs_transcoding| is true, this data must be
// decoded in a sandbox process.
using ImageDataCallback =
    base::OnceCallback<void(bool needs_transcoding, std::string)>;

// Returns bool success when the underlying storage completes an operation.
using ImageStoreOperationCallback = base::OnceCallback<void(bool)>;

// CachedImageMetadataProto will be returned if image metadata is loaded
// successfully.
using ImageMetadataCallback =
    base::OnceCallback<void(base::Optional<CachedImageMetadataProto>)>;

// Returns a vector of keys.
using KeysCallback = base::OnceCallback<void(std::vector<std::string>)>;

}  // namespace image_fetcher

#endif  // COMPONENTS_IMAGE_FETCHER_CORE_CACHE_IMAGE_STORE_TYPES_H_
