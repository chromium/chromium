// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VALUE_STORE_TEST_VALUE_STORE_FACTORY_H_
#define COMPONENTS_VALUE_STORE_TEST_VALUE_STORE_FACTORY_H_

#include <map>
#include <memory>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "components/value_store/value_store_factory.h"

namespace value_store {

class ValueStore;

// Used for tests when a new test ValueStore is required. Will either open a
// database on disk (if path provided) returning a |LeveldbValueStore|.
// Otherwise a new |TestingValueStore| instance will be returned.
class TestValueStoreFactory : public ValueStoreFactory {
 public:
  TestValueStoreFactory();
  explicit TestValueStoreFactory(const base::FilePath& db_path);
  TestValueStoreFactory(const TestValueStoreFactory&) = delete;
  TestValueStoreFactory& operator=(const TestValueStoreFactory&) = delete;

  // ValueStoreFactory
  std::unique_ptr<ValueStore> CreateValueStore(
      const base::FilePath& directory,
      const std::string& uma_client_name) override;
  void DeleteValueStore(const base::FilePath& directory) override;
  bool HasValueStore(const base::FilePath& directory) override;

  // Return the last created |ValueStore|. Use with caution as this may return
  // a dangling pointer since the creator now owns the ValueStore which can be
  // deleted at any time.
  ValueStore* LastCreatedStore() const;
  // Return the previously created |ValueStore| in the given directory.
  ValueStore* GetExisting(const base::FilePath& directory) const;
  // Reset this class (as if just created).
  void Reset();

 private:
  ~TestValueStoreFactory() override;

  std::unique_ptr<ValueStore> CreateStore();

  base::FilePath db_path_;
  raw_ptr<ValueStore, AcrossTasksDanglingUntriaged> last_created_store_ =
      nullptr;

  // A mapping from directories to their ValueStore. None of these value
  // stores are owned by this factory, so care must be taken when calling
  // GetExisting.
  std::map<base::FilePath, raw_ptr<ValueStore, CtnExperimental>>
      value_store_map_;
};

}  // namespace value_store

#endif  // COMPONENTS_VALUE_STORE_TEST_VALUE_STORE_FACTORY_H_
