// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VALUE_STORE_VALUE_STORE_FACTORY_H_
#define COMPONENTS_VALUE_STORE_VALUE_STORE_FACTORY_H_

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"

namespace base {
class FilePath;
}

namespace value_store {

class ValueStore;

// Manages ValueStore instances.
//
// This factory creates the lower level stores that directly read/write to disk.
class ValueStoreFactory : public base::RefCountedThreadSafe<ValueStoreFactory> {
 public:
  // Creates a |ValueStore| to contain data for a specific app in the given
  // directory.
  virtual std::unique_ptr<ValueStore> CreateValueStore(
      const base::FilePath& directory,
      const std::string& uma_client_name) = 0;

  // Deletes the ValueStore in the specified directory.
  virtual void DeleteValueStore(const base::FilePath& directory) = 0;

  // Returns whether there a ValueStore stored in the specified directory.
  virtual bool HasValueStore(const base::FilePath& directory) = 0;

 protected:
  friend class base::RefCountedThreadSafe<ValueStoreFactory>;
  virtual ~ValueStoreFactory() = default;
};

}  // namespace value_store

#endif  // COMPONENTS_VALUE_STORE_VALUE_STORE_FACTORY_H_
