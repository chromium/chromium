// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_LOCAL_SEARCH_SERVICE_LOCAL_SEARCH_SERVICE_SYNC_PROXY_H_
#define CHROMEOS_COMPONENTS_LOCAL_SEARCH_SERVICE_LOCAL_SEARCH_SERVICE_SYNC_PROXY_H_

#include <map>

#include "chromeos/components/local_search_service/public/mojom/local_search_service_proxy.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

class PrefService;

namespace chromeos {
namespace local_search_service {

class LocalSearchServiceSync;
class IndexSyncProxy;
enum class IndexId;
enum class Backend;

class LocalSearchServiceSyncProxy : public mojom::LocalSearchServiceSyncProxy,
                                    public KeyedService {
 public:
  explicit LocalSearchServiceSyncProxy(
      LocalSearchServiceSync* local_search_service);
  ~LocalSearchServiceSyncProxy() override;

  LocalSearchServiceSyncProxy(const LocalSearchServiceSyncProxy&) = delete;
  LocalSearchServiceSyncProxy& operator=(const LocalSearchServiceSyncProxy) =
      delete;

  // mojom::LocalSearchServiceSyncProxy:
  void GetIndex(
      IndexId index_id,
      Backend backend,
      mojo::PendingReceiver<mojom::IndexSyncProxy> index_receiver) override;

  void BindReceiver(
      mojo::PendingReceiver<mojom::LocalSearchServiceSyncProxy> receiver);

  // The version below allows an out-of-process client to directly obtain an
  // Index using their own delegate that runs in C++.
  // 1. Client's delegate obtains LocalSearchServicProxy from
  // LocalSearchServiceSyncProxyFactory.
  // 2. Client's delegate calls GetIndex to obtain an Index and binds the
  // IndexSyncProxy remote
  //    to the IndexSyncProxy implementation.
  void GetIndex(IndexId index_id,
                Backend backend,
                PrefService* local_state,
                mojo::PendingReceiver<mojom::IndexSyncProxy> index_receiver);

 private:
  LocalSearchServiceSync* const service_;
  mojo::ReceiverSet<mojom::LocalSearchServiceSyncProxy> receivers_;
  std::map<IndexId, std::unique_ptr<IndexSyncProxy>> indices_;
};

}  // namespace local_search_service
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_LOCAL_SEARCH_SERVICE_LOCAL_SEARCH_SERVICE_SYNC_PROXY_H_
