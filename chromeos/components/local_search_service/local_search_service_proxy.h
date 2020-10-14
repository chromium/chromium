// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_LOCAL_SEARCH_SERVICE_LOCAL_SEARCH_SERVICE_PROXY_H_
#define CHROMEOS_COMPONENTS_LOCAL_SEARCH_SERVICE_LOCAL_SEARCH_SERVICE_PROXY_H_

#include <map>

#include "chromeos/components/local_search_service/mojom/local_search_service_proxy.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

class PrefService;

namespace chromeos {
namespace local_search_service {

class LocalSearchServiceSync;
class IndexProxy;
enum class IndexId;
enum class Backend;

class LocalSearchServiceProxy : public mojom::LocalSearchServiceProxy,
                                public KeyedService {
 public:
  explicit LocalSearchServiceProxy(
      LocalSearchServiceSync* local_search_service);
  ~LocalSearchServiceProxy() override;

  LocalSearchServiceProxy(const LocalSearchServiceProxy&) = delete;
  LocalSearchServiceProxy& operator=(const LocalSearchServiceProxy) = delete;

  // mojom::LocalSearchServiceProxy:
  void GetIndex(
      IndexId index_id,
      Backend backend,
      mojo::PendingReceiver<mojom::IndexProxy> index_receiver) override;

  void BindReceiver(
      mojo::PendingReceiver<mojom::LocalSearchServiceProxy> receiver);

  // The version below allows an out-of-process client to directly obtain an
  // Index using their own delegate that runs in C++.
  // 1. Client's delegate obtains LocalSearchServicProxy from
  // LocalSearchServiceProxyFactory.
  // 2. Client's delegate calls GetIndex to obtain an Index and binds the
  // IndexProxy remote
  //    to the IndexProxy implementation.
  void GetIndex(IndexId index_id,
                Backend backend,
                PrefService* local_state,
                mojo::PendingReceiver<mojom::IndexProxy> index_receiver);

 private:
  LocalSearchServiceSync* const service_;
  mojo::ReceiverSet<mojom::LocalSearchServiceProxy> receivers_;
  std::map<IndexId, std::unique_ptr<IndexProxy>> indices_;
};

}  // namespace local_search_service
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_LOCAL_SEARCH_SERVICE_LOCAL_SEARCH_SERVICE_PROXY_H_
