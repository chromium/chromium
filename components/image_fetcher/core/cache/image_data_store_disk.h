// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IMAGE_FETCHER_CORE_CACHE_IMAGE_DATA_STORE_DISK_H_
#define COMPONENTS_IMAGE_FETCHER_CORE_CACHE_IMAGE_DATA_STORE_DISK_H_

#include <string>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "components/image_fetcher/core/cache/image_data_store.h"
#include "components/image_fetcher/core/cache/image_store_types.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace image_fetcher {

// Saves/loads image data on disk. Performs i/o in a task runner.
class ImageDataStoreDisk : public ImageDataStore {
 public:
  // Stores the image data under the given |generic_storage_path|. The path will
  // be postfixed with a special directory.
  ImageDataStoreDisk(base::FilePath generic_storage_path,
                     scoped_refptr<base::SequencedTaskRunner> task_runner);

  ImageDataStoreDisk(const ImageDataStoreDisk&) = delete;
  ImageDataStoreDisk& operator=(const ImageDataStoreDisk&) = delete;

  ~ImageDataStoreDisk() override;

  // ImageDataStorage:
  void Initialize(base::OnceClosure callback) override;
  bool IsInitialized() override;
  void SaveImage(const std::string& key,
                 std::string data,
                 bool needs_transcoding) override;
  void LoadImage(const std::string& key,
                 bool needs_transcoding,
                 ImageDataCallback callback) override;
  void DeleteImage(const std::string& key) override;
  void GetAllKeys(KeysCallback callback) override;

 private:
  // Called after the store has been initialized in a task runner.
  void OnInitializationComplete(base::OnceClosure callback,
                                InitializationStatus initialization_status);

  // Called when data is loaded from disk.
  void OnImageLoaded(bool needs_transcoding,
                     ImageDataCallback callback,
                     std::string data);

  // Set to be INITIALIZED if a directory exists, or can be created under
  // |storage_path|. If initialization fails, there's no need to retry.
  InitializationStatus initialization_status_;

  // The base path that's available to the store. A postfix will be appended
  // to house the image data files themselves.
  base::FilePath storage_path_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::WeakPtrFactory<ImageDataStoreDisk> weak_ptr_factory_{this};
};
}  // namespace image_fetcher

#endif  // COMPONENTS_IMAGE_FETCHER_CORE_CACHE_IMAGE_DATA_STORE_DISK_H_
