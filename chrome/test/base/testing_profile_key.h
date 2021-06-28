// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_TESTING_PROFILE_KEY_H_
#define CHROME_TEST_BASE_TESTING_PROFILE_KEY_H_

#include "chrome/browser/profiles/profile_key.h"
#include "chrome/test/base/testing_profile.h"

// ProfileKey to be used with TestingProfile.
class TestingProfileKey : public ProfileKey {
 public:
  TestingProfileKey(TestingProfile* testing_profile,
                    const base::FilePath& path,
                    ProfileKey* original_key = nullptr);
  ~TestingProfileKey() override;

  // Instead of holding a ProtoDatabaseProvider pointer it retrieves a DB
  // Provider from |testing_profile_|'s default StoragePartition. This is done
  // because TestingProfile creates its default StoragePartition lazily, as
  // opposed to ProfileImpl, which creates it on its constructor. Creating
  // StoragePartition in constructor of testing profile is unnecessary for tests
  // that do not require it.
  leveldb_proto::ProtoDatabaseProvider* GetProtoDatabaseProvider() override;

 private:
  TestingProfile* testing_profile_ = nullptr;
};

#endif  // CHROME_TEST_BASE_TESTING_PROFILE_KEY_H_
