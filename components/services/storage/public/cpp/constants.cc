// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/public/cpp/constants.h"

namespace storage {

// The base path where StorageBuckets data is persisted on disk, relative to a
// storage partition's root directory.
const base::FilePath::CharType kWebStorageDirectory[] =
    FILE_PATH_LITERAL("WebStorage");

// The path where Local Storage data is persisted on disk, relative to a storage
// partition's root directory.
const base::FilePath::CharType kLocalStoragePath[] =
    FILE_PATH_LITERAL("Local Storage");

// The name of the Leveldb database to use for databases persisted on disk.
const char kLocalStorageLeveldbName[] = "leveldb";

// The path where service worker and cache storage data are persisted on disk,
// relative to a storage partition's root directory.
const base::FilePath::CharType kServiceWorkerDirectory[] =
    FILE_PATH_LITERAL("Service Worker");

// The path where media license data is persisted on disk, relative to the path
// for the respective storage bucket.
const base::FilePath::CharType kMediaLicenseDirectory[] =
    FILE_PATH_LITERAL("Media Licenses");

// The file name of the database storing media license data.
const base::FilePath::CharType kMediaLicenseDatabaseFileName[] =
    FILE_PATH_LITERAL("Media Licenses.db");

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

}  // namespace storage
