// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_STORE_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_STORE_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "base/strings/string_piece.h"

namespace data_reduction_proxy {

// Interface for a permanent key/value store used by the Data Reduction Proxy
// component. This class has no-op methods for clients that do not want to
// support a permanent store.
class DataStore {
 public:
  // Values are used in UMA. Do not change existing values; only append to the
  // end. STATUS_MAX should always be last element.
  enum Status { OK, NOT_FOUND, CORRUPTED, IO_ERROR, MISC_ERROR, STATUS_MAX };

  DataStore();

  DataStore(const DataStore&) = delete;
  DataStore& operator=(const DataStore&) = delete;

  virtual ~DataStore();

  // Initializes the store on DB sequenced task runner.
  virtual void InitializeOnDBThread();

  // Gets the value from the store for the provided key.
  virtual Status Get(base::StringPiece key, std::string* value);

  // Persists the provided keys and values into the store.
  virtual Status Put(const std::map<std::string, std::string>& map);

  virtual Status Delete(base::StringPiece key);

  // Deletes the LevelDB and recreates it.
  virtual Status RecreateDB();
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_STORE_H_
