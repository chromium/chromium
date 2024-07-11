// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/value_store/test_value_store_factory.h"

#include "base/containers/contains.h"
#include "base/memory/ptr_util.h"
#include "base/not_fatal_until.h"
#include "components/value_store/leveldb_value_store.h"
#include "components/value_store/testing_value_store.h"

namespace {

const char kUMAClientName[] = "Test";

}  // namespace

namespace value_store {

TestValueStoreFactory::TestValueStoreFactory() = default;

TestValueStoreFactory::TestValueStoreFactory(const base::FilePath& db_path)
    : db_path_(db_path) {}

TestValueStoreFactory::~TestValueStoreFactory() = default;

std::unique_ptr<ValueStore> TestValueStoreFactory::CreateValueStore(
    const base::FilePath& directory,
    const std::string& uma_client_name) {
  std::unique_ptr<ValueStore> value_store(CreateStore());
  // This factory is purposely keeping the raw pointers to each ValueStore
  // created. Tests using TestValueStoreFactory must be careful to keep
  // those ValueStore's alive for the duration of their test.
  value_store_map_[directory] = value_store.get();
  return value_store;
}

ValueStore* TestValueStoreFactory::LastCreatedStore() const {
  return last_created_store_;
}

void TestValueStoreFactory::DeleteValueStore(const base::FilePath& directory) {
  value_store_map_.erase(directory);
}

bool TestValueStoreFactory::HasValueStore(const base::FilePath& directory) {
  return base::Contains(value_store_map_, directory);
}

ValueStore* TestValueStoreFactory::GetExisting(
    const base::FilePath& directory) const {
  auto it = value_store_map_.find(directory);
  CHECK(it != value_store_map_.end(), base::NotFatalUntil::M130);
  return it->second;
}

void TestValueStoreFactory::Reset() {
  last_created_store_ = nullptr;
  value_store_map_.clear();
}

std::unique_ptr<ValueStore> TestValueStoreFactory::CreateStore() {
  std::unique_ptr<ValueStore> store;
  if (db_path_.empty())
    store = std::make_unique<TestingValueStore>();
  else
    store = std::make_unique<LeveldbValueStore>(kUMAClientName, db_path_);
  last_created_store_ = store.get();
  return store;
}

}  // namespace value_store
