// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IMAGE_FETCHER_CORE_CACHE_IMAGE_DATA_STORE_H_
#define COMPONENTS_IMAGE_FETCHER_CORE_CACHE_IMAGE_DATA_STORE_H_

#include <string>

#include "components/image_fetcher/core/cache/image_store_types.h"

namespace image_fetcher {

// Interface for an object capable of saving/loading image data.
class ImageDataStore {
 public:
  virtual ~ImageDataStore() = default;

  // Initialize this store. If calls are made to the class before the
  // initialization has completed, they are ignored. Ignored requests will
  // return empty data. It's the responsibility of the caller to check for
  // initialization before calling.
  virtual void Initialize(base::OnceClosure callback) = 0;

  // Returns true if initialization has finished successfully, else false.
  // While this is false, initialization may have already started.
  virtual bool IsInitialized() = 0;

  // Adds or updates the image data for the |key|.
  virtual void SaveImage(const std::string& key,
                         std::string image_data,
                         bool needs_transcoding) = 0;

  // Loads the image data for the |key| and passes it to |callback|. If the
  // image isn't available, empty data will be returned.
  virtual void LoadImage(const std::string& key,
                         bool needs_transcoding,
                         ImageDataCallback callback) = 0;

  // Deletes the image data for the |key|.
  virtual void DeleteImage(const std::string& key) = 0;

  // Returns all the key this store has.
  virtual void GetAllKeys(KeysCallback callback) = 0;
};
}  // namespace image_fetcher

#endif  // COMPONENTS_IMAGE_FETCHER_CORE_CACHE_IMAGE_DATA_STORE_H_
