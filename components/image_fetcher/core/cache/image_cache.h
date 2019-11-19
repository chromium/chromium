// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IMAGE_FETCHER_CORE_CACHE_IMAGE_CACHE_H_
#define COMPONENTS_IMAGE_FETCHER_CORE_CACHE_IMAGE_CACHE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "components/image_fetcher/core/cache/image_store_types.h"
#include "components/image_fetcher/core/cache/proto/cached_image_metadata.pb.h"

class PrefRegistrySimple;
class PrefService;

namespace base {
class Clock;
class SequencedTaskRunner;
}  // namespace base

namespace image_fetcher {

class ImageCache;
class ImageDataStore;
class ImageMetadataStore;

// Persist image meta/data via the given implementations of ImageDataStore and
// ImageMetadataStore.
class ImageCache : public base::RefCounted<ImageCache> {
 public:
  static std::string HashUrlToKey(const std::string& input);
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  ImageCache(std::unique_ptr<ImageDataStore> data_storage,
             std::unique_ptr<ImageMetadataStore> metadata_storage,
             PrefService* pref_service,
             base::Clock* clock,
             scoped_refptr<base::SequencedTaskRunner> task_runner);

  // Adds or updates the image data for the |url|. If the class hasn't been
  // initialized yet, the call is queued.
  void SaveImage(std::string url,
                 std::string image_data,
                 bool needs_transcoding);

  // Loads the image data for the |url| and passes it to |callback|. If there's
  // no image in the cache, then an empty string is returned. If |read_only|
  // is true, then the cache metadata won't be updated.
  void LoadImage(bool read_only, std::string url, ImageDataCallback callback);

  // Deletes the image data for the |url|.
  void DeleteImage(std::string url);

 private:
  friend class CachedImageFetcherImageCacheTest;
  friend class base::RefCounted<ImageCache>;
  ~ImageCache();

  // Queue or start |request| depending if the cache is initialized.
  void QueueOrStartRequest(base::OnceClosure request);
  // Start initializing the stores if it hasn't been started already.
  void MaybeStartInitialization();
  // Returns true iff both the stores have been initialized.
  bool AreAllDependenciesInitialized() const;
  // Receives callbacks when stores are initialized.
  void OnDependencyInitialized();

  // Saves the |image_data| for |url|.
  void SaveImageImpl(const std::string& url,
                     std::string image_data,
                     bool needs_transcoding);
  // Loads the data for |url|, calls the user back before updating metadata.
  void LoadImageImpl(bool read_only,
                     const std::string& url,
                     ImageDataCallback callback);
  // Loads the image metadata for the given |url|, used to inspect if there's
  // data available on disk that's in need of transcoding.
  void OnImageMetadataLoadedForLoadImage(
      bool read_only,
      const std::string& key,
      ImageDataCallback callback,
      base::TimeTicks start_time,
      base::Optional<CachedImageMetadataProto> metadata);
  // Deletes the data for |url|.
  void DeleteImageImpl(const std::string& url);

  // Runs eviction on the data stores (run on startup).
  void RunEvictionOnStartup();
  // Runs eviction on the data stores (run when cache is full).
  void RunEvictionWhenFull();
  // Catch-all method for eviction, runs reconciliation routine after if
  // |run_reconciliation| is specified. Evicts until there are |bytes_left|
  // left in storage.
  void RunEviction(size_t bytes_left, base::OnceClosure on_completion);
  // Deletes the given keys from the data store.
  void OnKeysEvicted(base::OnceClosure on_completion,
                     std::vector<std::string> keys);
  // Reconcile what's on disk against what's in metadata. Any mismatches will
  // result in eviction.
  void RunReconciliation();
  void ReconcileMetadataKeys(std::vector<std::string> md_keys);
  void ReconcileDataKeys(std::vector<std::string> md_keys,
                         std::vector<std::string> data_keys);

  bool initialization_attempted_;
  std::vector<base::OnceClosure> queued_requests_;

  std::unique_ptr<ImageDataStore> data_store_;
  std::unique_ptr<ImageMetadataStore> metadata_store_;
  PrefService* pref_service_;

  // Owned by the service which instantiates this.
  base::Clock* clock_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::WeakPtrFactory<ImageCache> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ImageCache);
};

}  // namespace image_fetcher

#endif  // COMPONENTS_IMAGE_FETCHER_CORE_CACHE_IMAGE_CACHE_H_
