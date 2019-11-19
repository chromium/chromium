// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/testing_profile_key.h"

#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

TestingProfileKey::TestingProfileKey(TestingProfile* testing_profile,
                                     const base::FilePath& path,
                                     ProfileKey* original_key)
    : ProfileKey(path, original_key), testing_profile_(testing_profile) {}

TestingProfileKey::~TestingProfileKey() = default;

leveldb_proto::ProtoDatabaseProvider*
TestingProfileKey::GetProtoDatabaseProvider() {
  auto* storage_partition =
      content::BrowserContext::GetDefaultStoragePartition(testing_profile_);
  return storage_partition->GetProtoDatabaseProvider();
}
