// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_QUERY_TILES_INTERNAL_STORE_H_
#define COMPONENTS_QUERY_TILES_INTERNAL_STORE_H_

#include <map>
#include <memory>
#include <string>

#include "base/functional/callback.h"

namespace query_tiles {

// Interface of query tile collection store.
template <typename T>
class Store {
 public:
  using KeysAndEntries = std::map<std::string, std::unique_ptr<T>>;
  using LoadCallback = base::OnceCallback<void(bool, KeysAndEntries)>;
  using UpdateCallback = base::OnceCallback<void(bool)>;
  using DeleteCallback = base::OnceCallback<void(bool)>;

  // Initializes the database and loads all entries into memory.
  virtual void InitAndLoad(LoadCallback callback) = 0;

  // Adds/Updates an existing entry.
  virtual void Update(const std::string& key,
                      const T& entry,
                      UpdateCallback callback) = 0;

  // Deletes an entry from database.
  virtual void Delete(const std::string& key, DeleteCallback callback) = 0;

  Store() = default;
  virtual ~Store() = default;

  Store(const Store& other) = delete;
  Store& operator=(const Store& other) = delete;
};

}  // namespace query_tiles

#endif  // COMPONENTS_QUERY_TILES_INTERNAL_STORE_H_
