// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IMAGE_FETCHER_CORE_CACHE_IMAGE_STORE_TYPES_H_
#define COMPONENTS_IMAGE_FETCHER_CORE_CACHE_IMAGE_STORE_TYPES_H_

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "components/image_fetcher/core/cache/proto/cached_image_metadata.pb.h"

namespace base {
class TimeDelta;
}

namespace image_fetcher {

// Represents the initialization status of a image storage module.
enum class InitializationStatus {
  UNINITIALIZED,
  INITIALIZED,
  INIT_FAILURE,
};

// Controls how cached image fetcher manages disk cache files. Maps to
// CacheStrategy in cached_image_metadata.proto.
enum class CacheOption {
  kBestEffort = 0,
  kHoldUntilExpired = 1,
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
    base::OnceCallback<void(std::optional<CachedImageMetadataProto>)>;

// Returns a vector of keys.
using KeysCallback = base::OnceCallback<void(std::vector<std::string>)>;

// The expiration interval for CacheStrategy::HOLD_UNTIL_EXPIRED.
using ExpirationInterval = std::optional<base::TimeDelta>;

}  // namespace image_fetcher

#endif  // COMPONENTS_IMAGE_FETCHER_CORE_CACHE_IMAGE_STORE_TYPES_H_
