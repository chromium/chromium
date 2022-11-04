// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VALUE_STORE_VALUE_STORE_FACTORY_IMPL_H_
#define COMPONENTS_VALUE_STORE_VALUE_STORE_FACTORY_IMPL_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "components/value_store/value_store.h"
#include "components/value_store/value_store_factory.h"

namespace value_store {

// A factory to create ValueStore instances.
class ValueStoreFactoryImpl : public ValueStoreFactory {
 public:
  explicit ValueStoreFactoryImpl(const base::FilePath& profile_path);
  ValueStoreFactoryImpl(const ValueStoreFactoryImpl&) = delete;
  ValueStoreFactoryImpl& operator=(const ValueStoreFactoryImpl&) = delete;

  // ValueStoreFactory:
  std::unique_ptr<ValueStore> CreateValueStore(
      const base::FilePath& directory,
      const std::string& uma_client_name) override;
  void DeleteValueStore(const base::FilePath& directory) override;
  bool HasValueStore(const base::FilePath& directory) override;

 private:
  ~ValueStoreFactoryImpl() override;

  base::FilePath GetDBPath(const base::FilePath& directory) const;

  const base::FilePath profile_path_;
};

}  // namespace value_store

#endif  // COMPONENTS_VALUE_STORE_VALUE_STORE_FACTORY_IMPL_H_
