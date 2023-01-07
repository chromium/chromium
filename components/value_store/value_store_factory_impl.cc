// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/value_store/value_store_factory_impl.h"

#include <memory>
#include <string>

#include "base/files/file_util.h"
#include "components/value_store/leveldb_value_store.h"

namespace value_store {

ValueStoreFactoryImpl::ValueStoreFactoryImpl(const base::FilePath& profile_path)
    : profile_path_(profile_path) {}

ValueStoreFactoryImpl::~ValueStoreFactoryImpl() = default;

base::FilePath ValueStoreFactoryImpl::GetDBPath(
    const base::FilePath& directory) const {
  DCHECK(!directory.empty());
  return profile_path_.Append(directory);
}

std::unique_ptr<ValueStore> ValueStoreFactoryImpl::CreateValueStore(
    const base::FilePath& directory,
    const std::string& uma_client_name) {
  return std::make_unique<LeveldbValueStore>(uma_client_name,
                                             GetDBPath(directory));
}

void ValueStoreFactoryImpl::DeleteValueStore(const base::FilePath& directory) {
  base::DeletePathRecursively(GetDBPath(directory));
}

bool ValueStoreFactoryImpl::HasValueStore(const base::FilePath& directory) {
  return base::DirectoryExists(GetDBPath(directory));
}

}  // namespace value_store
