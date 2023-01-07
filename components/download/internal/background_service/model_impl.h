// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_MODEL_IMPL_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_MODEL_IMPL_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/download/internal/background_service/model.h"
#include "components/download/internal/background_service/store.h"
#include "components/download/public/background_service/clients.h"

namespace download {

struct Entry;

// The internal implementation of Model.
class ModelImpl : public Model {
 public:
  ModelImpl(std::unique_ptr<Store> store);

  ModelImpl(const ModelImpl&) = delete;
  ModelImpl& operator=(const ModelImpl&) = delete;

  ~ModelImpl() override;

  // Model implementation.
  void Initialize(Client* client) override;
  void HardRecover() override;
  void Add(const Entry& entry) override;
  void Update(const Entry& entry) override;
  void Remove(const std::string& guid) override;
  Entry* Get(const std::string& guid) override;
  EntryList PeekEntries() override;
  size_t EstimateMemoryUsage() const override;

 private:
  using OwnedEntryMap = std::map<std::string, std::unique_ptr<Entry>>;

  void OnInitializedFinished(bool success,
                             std::unique_ptr<std::vector<Entry>> entries);
  void OnHardRecoverFinished(bool success);
  void OnAddFinished(DownloadClient client,
                     const std::string& guid,
                     bool success);
  void OnUpdateFinished(DownloadClient client,
                        const std::string& guid,
                        bool success);
  void OnRemoveFinished(DownloadClient client,
                        const std::string& guid,
                        bool success);

  // The external Model::Client reference that will receive all interesting
  // Model notifications.
  raw_ptr<Client> client_;

  // The backing Store that is responsible for saving and loading the
  // persisted entries.
  std::unique_ptr<Store> store_;

  // A map of [guid] -> [std::unique_ptr<Entry>].  Effectively the cache of the
  // entries saved in Store.
  OwnedEntryMap entries_;

  base::WeakPtrFactory<ModelImpl> weak_ptr_factory_{this};
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_MODEL_IMPL_H_
