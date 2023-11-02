// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/content/mock_file_system_helper.h"

#include "base/callback.h"
#include "base/containers/contains.h"
#include "components/browsing_data/content/file_system_helper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace browsing_data {

MockFileSystemHelper::MockFileSystemHelper(
    content::BrowserContext* browser_context)
    : FileSystemHelper(
          browser_context->GetDefaultStoragePartition()->GetFileSystemContext(),
          {},
          browser_context->GetDefaultStoragePartition()->GetNativeIOContext()) {
}

MockFileSystemHelper::~MockFileSystemHelper() {}

void MockFileSystemHelper::StartFetching(FetchCallback callback) {
  ASSERT_FALSE(callback.is_null());
  ASSERT_TRUE(callback_.is_null());
  callback_ = std::move(callback);
}

void MockFileSystemHelper::DeleteFileSystemOrigin(const url::Origin& origin) {
  std::string key = origin.Serialize();
  ASSERT_TRUE(base::Contains(file_systems_, key));
  last_deleted_origin_ = origin;
  file_systems_[key] = false;
}

void MockFileSystemHelper::AddFileSystem(const url::Origin& origin,
                                         bool has_persistent,
                                         bool has_temporary,
                                         bool has_syncable,
                                         int64_t size_persistent,
                                         int64_t size_temporary,
                                         int64_t size_syncable) {
  FileSystemHelper::FileSystemInfo info(origin);
  if (has_persistent)
    info.usage_map[storage::kFileSystemTypePersistent] = size_persistent;
  if (has_temporary)
    info.usage_map[storage::kFileSystemTypeTemporary] = size_temporary;
  if (has_syncable)
    info.usage_map[storage::kFileSystemTypeSyncable] = size_syncable;
  response_.push_back(info);
  file_systems_[origin.Serialize()] = true;
}

void MockFileSystemHelper::AddFileSystemSamples() {
  AddFileSystem(url::Origin::Create(GURL("http://fshost1:1")), false, true,
                false, 0, 1, 0);
  AddFileSystem(url::Origin::Create(GURL("http://fshost2:2")), true, false,
                true, 2, 0, 2);
  AddFileSystem(url::Origin::Create(GURL("http://fshost3:3")), true, true, true,
                3, 3, 3);
}

void MockFileSystemHelper::Notify() {
  std::move(callback_).Run(response_);
}

void MockFileSystemHelper::Reset() {
  for (auto& pair : file_systems_)
    pair.second = true;
}

bool MockFileSystemHelper::AllDeleted() {
  for (const auto& pair : file_systems_) {
    if (pair.second)
      return false;
  }
  return true;
}

}  // namespace browsing_data
