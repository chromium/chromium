// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IMAGE_FETCHER_CORE_CACHE_IMAGE_METADATA_STORE_H_
#define COMPONENTS_IMAGE_FETCHER_CORE_CACHE_IMAGE_METADATA_STORE_H_

#include <string>

#include "base/time/time.h"
#include "components/image_fetcher/core/cache/image_store_types.h"

namespace image_fetcher {

// Interface for an object capable of saving/loading image metadata.
class ImageMetadataStore {
 public:
  virtual ~ImageMetadataStore() = default;

  // Initialize this store. If calls are made to the class before the
  // initialization has completed, they are ignored. Ignored requests won't do
  // any meaningful work. It's the responsibility of the caller to check for
  // initialization before calling.
  virtual void Initialize(base::OnceClosure callback) = 0;

  // Returns true if initialization has finished successfully, else false.
  // While this is false, initialization may have already started.
  virtual bool IsInitialized() = 0;

  // Loads the image metadata for the |key|.
  virtual void LoadImageMetadata(const std::string& key,
                                 ImageMetadataCallback) = 0;

  // Adds or updates the image metadata for the |key|. If metadata exists for an
  // image and the |needs_transcoding| is still true, we don't need to update
  // the existing metadata.
  virtual void SaveImageMetadata(const std::string& key,
                                 const size_t data_size,
                                 bool needs_transcoding,
                                 ExpirationInterval expiration_interval) = 0;

  // Deletes the image metadata for the |key|.
  virtual void DeleteImageMetadata(const std::string& key) = 0;

  // Updates |last_used_time| for the given |key| if it exists.
  virtual void UpdateImageMetadata(const std::string& key) = 0;

  // Returns all the keys this store has.
  virtual void GetAllKeys(KeysCallback callback) = 0;

  // Returns the total size of what's in metadata for a given cache option,
  // possibly incorrect.
  virtual int64_t GetEstimatedSize(CacheOption cache_option) = 0;

  // Deletes all metadata that's been cached before the boundary given as
  // |expiration_time|.
  void EvictImageMetadata(base::Time expiration_time, KeysCallback callback) {
    EvictImageMetadata(expiration_time, /* Max size_t */ -1,
                       std::move(callback));
  }

  // Deletes all metadata that's been cached before the boundary given as
  // |expiration_time|. Evicts other metadata until there are |bytes_left|
  // in storage.
  virtual void EvictImageMetadata(base::Time expiration_time,
                                  const size_t bytes_left,
                                  KeysCallback callback) = 0;
};
}  // namespace image_fetcher

#endif  // COMPONENTS_IMAGE_FETCHER_CORE_CACHE_IMAGE_METADATA_STORE_H_
