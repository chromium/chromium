// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_STORE_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_STORE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"

namespace download {

struct Entry;

// A backing storage interface responsible for persisting Entry objects.
class Store {
 public:
  using InitCallback =
      base::OnceCallback<void(bool success,
                              std::unique_ptr<std::vector<Entry>> entries)>;
  using StoreCallback = base::OnceCallback<void(bool success)>;

  virtual ~Store() = default;

  // Returns whether or not this Store is initialized and can be interracted
  // with.
  virtual bool IsInitialized() = 0;

  // Initializes this Store and asynchronously returns whether or not that
  // initialization was successful as well as a list of Entry objects from the
  // Store.
  virtual void Initialize(InitCallback callback) = 0;

  // Destroys the underlying store and attempts to re-initialize.
  virtual void HardRecover(StoreCallback callback) = 0;

  // Adds or updates |entry| in this Store asynchronously and returns whether or
  // not that was successful.
  virtual void Update(const Entry& entry, StoreCallback callback) = 0;

  // Removes the Entry associated with |guid| from this Store asynchronously and
  // returns whether or not that was successful.
  virtual void Remove(const std::string& guid, StoreCallback callback) = 0;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_STORE_H_
