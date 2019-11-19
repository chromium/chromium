// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/public/cpp/constants.h"

namespace storage {

// The path where Local Storage data is persisted on disk, relative to a storage
// partition's root directory.
const base::FilePath::CharType kLocalStoragePath[] =
    FILE_PATH_LITERAL("Local Storage");

// The name of the Leveldb database to use for databases persisted on disk.
const char kLocalStorageLeveldbName[] = "leveldb";

}  // namespace storage
