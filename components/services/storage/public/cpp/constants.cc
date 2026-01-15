// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/public/cpp/constants.h"

#include "components/services/storage/dom_storage/features.h"

namespace storage {

// The base path where StorageBuckets data is persisted on disk, relative to a
// storage partition's root directory.
const base::FilePath::CharType kWebStorageDirectory[] =
    FILE_PATH_LITERAL("WebStorage");

// The path where service worker and cache storage data are persisted on disk,
// relative to a storage partition's root directory.
const base::FilePath::CharType kServiceWorkerDirectory[] =
    FILE_PATH_LITERAL("Service Worker");

// The path where File System data is persisted on disk for partitioned storage.
const base::FilePath::CharType kFileSystemDirectory[] =
    FILE_PATH_LITERAL("FileSystem");

// The path where IndexedDB data is persisted on disk for partitioned storage.
const base::FilePath::CharType kIndexedDbDirectory[] =
    FILE_PATH_LITERAL("IndexedDB");

// The path where BackgroundFetch data is persisted on disk for partitioned
// storage.
const base::FilePath::CharType kBackgroundFetchDirectory[] =
    FILE_PATH_LITERAL("BackgroundFetch");

// The path where CacheStorage data is persisted on disk for partitioned
// storage.
const base::FilePath::CharType kCacheStorageDirectory[] =
    FILE_PATH_LITERAL("CacheStorage");

// The path where ServiceWorker script data is persisted on disk for partitioned
// storage.
const base::FilePath::CharType kScriptCacheDirectory[] =
    FILE_PATH_LITERAL("ScriptCache");

// The path where Shared Storage data is persisted on disk, relative to a
// storage partition's root directory.
const base::FilePath::CharType kSharedStoragePath[] =
    FILE_PATH_LITERAL("SharedStorage");

base::FilePath GetLocalStorageDatabasePath(
    const base::FilePath& storage_partition_dir) {
  CHECK(!storage_partition_dir.empty());
  CHECK(storage_partition_dir.IsAbsolute());

  if (base::FeatureList::IsEnabled(kDomStorageSqlite)) {
    return storage_partition_dir.AppendASCII("LocalStorage");
  }
  return storage_partition_dir.AppendASCII("Local Storage")
      .AppendASCII("leveldb");
}

}  // namespace storage
