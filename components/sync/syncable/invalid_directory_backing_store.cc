// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/syncable/invalid_directory_backing_store.h"

#include <string>

#include "base/bind.h"

namespace syncer {
namespace syncable {

InvalidDirectoryBackingStore::InvalidDirectoryBackingStore()
    : DirectoryBackingStore("some_fake_user",
                            base::BindRepeating([]() -> std::string {
                              return "some_fake_cache_guid";
                            })) {}

InvalidDirectoryBackingStore::~InvalidDirectoryBackingStore() {}

DirOpenResult InvalidDirectoryBackingStore::Load(
    Directory::MetahandlesMap* handles_map,
    MetahandleSet* metahandles_to_purge,
    Directory::KernelLoadInfo* kernel_load_info) {
  return FAILED_OPEN_DATABASE;
}

}  // namespace syncable
}  // namespace syncer
