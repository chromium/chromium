// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_PUBLIC_PERSISTENT_KEY_VALUE_STORE_H_
#define COMPONENTS_FEED_CORE_V2_PUBLIC_PERSISTENT_KEY_VALUE_STORE_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"

namespace feed {

// A generic persistent key-value cache. Has a maximum size determined by
// `feed::Config`. Once size of all values exceed the maximum, older keys
// are eventually evicted. Key age is determined only by the last call to
// `Put()`.
class PersistentKeyValueStore {
 public:
  struct Result {
    Result();
    Result(Result&&);
    Result& operator=(Result&&);
    ~Result();
    // Whether the operation succeeded. Failure may be due to a low level
    // database error, or a missing key/value pair.
    bool success = false;
    // For `Get()` operations, the value of the key if it exists.
    std::optional<std::string> get_result;
  };

  using ResultCallback = base::OnceCallback<void(Result)>;

  PersistentKeyValueStore() = default;
  virtual ~PersistentKeyValueStore() = default;
  PersistentKeyValueStore(const PersistentKeyValueStore&) = delete;
  PersistentKeyValueStore& operator=(const PersistentKeyValueStore&) = delete;

  // Erase all data in the store.
  virtual void ClearAll(ResultCallback callback) = 0;
  // Write/overwrite a key/value pair.
  virtual void Put(const std::string& key,
                   const std::string& value,
                   ResultCallback callback) = 0;
  // Get a value by key.
  virtual void Get(const std::string& key, ResultCallback callback) = 0;
  // Delete a value by key.
  virtual void Delete(const std::string& key, ResultCallback callback) = 0;

 private:
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_PUBLIC_PERSISTENT_KEY_VALUE_STORE_H_
