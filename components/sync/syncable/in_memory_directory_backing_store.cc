// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/syncable/in_memory_directory_backing_store.h"

namespace syncer {
namespace syncable {

InMemoryDirectoryBackingStore::InMemoryDirectoryBackingStore(
    const std::string& dir_name,
    const base::RepeatingCallback<std::string()>& cache_guid_generator)
    : DirectoryBackingStore(dir_name, cache_guid_generator) {}

DirOpenResult InMemoryDirectoryBackingStore::Load(
    Directory::MetahandlesMap* handles_map,
    MetahandleSet* metahandles_to_purge,
    Directory::KernelLoadInfo* kernel_load_info) {
  if (!IsOpen()) {
    if (!OpenInMemory())
      return FAILED_OPEN_DATABASE;
  }

  bool did_start_new = false;
  if (!InitializeTables(&did_start_new))
    return FAILED_OPEN_DATABASE;

  if (!LoadEntries(handles_map, metahandles_to_purge))
    return FAILED_DATABASE_CORRUPT;
  if (!LoadInfo(kernel_load_info))
    return FAILED_DATABASE_CORRUPT;
  if (!VerifyReferenceIntegrity(handles_map))
    return FAILED_DATABASE_CORRUPT;

  return did_start_new ? OPENED_NEW : OPENED_EXISTING;
}

}  // namespace syncable
}  // namespace syncer
