// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_MODEL_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_MODEL_H_

#include <stddef.h>
#include <memory>
#include <string>
#include <vector>

#include "components/download/internal/background_service/entry.h"
#include "components/download/public/background_service/clients.h"

namespace download {

class Store;

// The model that contains a runtime representation of Entry entries. Any
// updates to the model will be persisted to a backing |Store| as necessary.
class Model {
 public:
  // The Client which is responsible for handling all relevant messages from the
  // model.
  class Client {
   public:
    virtual ~Client() = default;

    // Called asynchronously in response to a Model::Initialize call.  If
    // |success| is |false|, initialization of the Model and/or the underlying
    // Store failed.  Initialization of the Model is complete after this
    // callback.  If |success| is true it can be accessed now.
    virtual void OnModelReady(bool success) = 0;

    // Called asynchronously in response to a Model::HardRecover call.  If
    // |success| is |false|, recovery of the Model and/or the underlying Store
    // failed.  After this call there should be no entries stored in this Model.
    virtual void OnModelHardRecoverComplete(bool success) = 0;

    // Called when an Entry addition is complete.  |success| determines whether
    // or not the entry has been successfully persisted to the Store.
    virtual void OnItemAdded(bool success,
                             DownloadClient client,
                             const std::string& guid) = 0;

    // Called when an Entry update is complete.  |success| determines whether or
    // not the update has been successfully persisted to the Store.
    virtual void OnItemUpdated(bool success,
                               DownloadClient client,
                               const std::string& guid) = 0;

    // Called when an Entry removal is complete.  |success| determines whether
    // or not the entry has been successfully removed from the Store.
    virtual void OnItemRemoved(bool success,
                               DownloadClient client,
                               const std::string& guid) = 0;
  };

  using EntryList = std::vector<Entry*>;

  virtual ~Model() = default;

  // Initializes the Model.  Client::OnInitialized() will be called in response.
  // The Model can be used after that call.
  virtual void Initialize(Client* client) = 0;

  // Deletes and attempts to re-initialize the Store.
  // Client::OnHardRecoveryComplete() will be called in response asynchronously.
  virtual void HardRecover() = 0;

  // Adds |entry| to this Model and attempts to write |entry| to the Store.
  // Client::OnItemAdded() will be called in response asynchronously.
  virtual void Add(const Entry& entry) = 0;

  // Updates |entry| in this Model and attempts to write |entry| to the Store.
  // Client::OnItemUpdated() will be called in response asynchronously.
  virtual void Update(const Entry& entry) = 0;

  // Removes the Entry specified by |guid| from this Model and attempts to
  // remove that entry from the Store.  Client::OnItemRemoved() will be called
  // in response asynchronously.
  virtual void Remove(const std::string& guid) = 0;

  // Retrieves an Entry identified by |guid| or |nullptr| if no entry exists.
  // IMPORTANT NOTE: The result of this method should be used immediately and
  // NOT stored.  The underlying data may get updated or removed by any other
  // modifications to this model.
  virtual Entry* Get(const std::string& guid) = 0;

  // Returns a temporary list of Entry objects that this Model stores.
  // IMPORTANT NOTE: Like Get(), the result of this method should be used
  // immediately and NOT stored.  The underlying data may get updated or removed
  // by any other modifications to this model.
  virtual EntryList PeekEntries() = 0;

  // Returns the estimate of dynamically allocated memory in bytes.
  virtual size_t EstimateMemoryUsage() const = 0;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_MODEL_H_
