// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOCAL_SEARCH_SERVICE_PUBLIC_CPP_LOCAL_SEARCH_SERVICE_PROXY_H_
#define CHROMEOS_ASH_COMPONENTS_LOCAL_SEARCH_SERVICE_PUBLIC_CPP_LOCAL_SEARCH_SERVICE_PROXY_H_

#include <memory>

#include "chromeos/ash/components/local_search_service/public/cpp/local_search_service_provider.h"
#include "chromeos/ash/components/local_search_service/public/mojom/index.mojom.h"
#include "chromeos/ash/components/local_search_service/public/mojom/local_search_service.mojom.h"
#include "chromeos/ash/components/local_search_service/search_metrics_reporter.h"
#include "components/keyed_service/core/keyed_service.h"

class PrefService;

namespace ash::local_search_service {

class LocalSearchServiceProxy : public KeyedService {
 public:
  explicit LocalSearchServiceProxy(bool for_testing = false);
  ~LocalSearchServiceProxy() override;

  LocalSearchServiceProxy(const LocalSearchServiceProxy&) = delete;
  LocalSearchServiceProxy& operator=(const LocalSearchServiceProxy&) = delete;

  // This function will be called by LocalSearchServiceClient or PreProfileInit.
  // Note: |local_state| will be shared by all LSS clients and it's used to
  // create daily metrics reporter.
  void SetLocalState(PrefService* local_state);

  // A client will call this function to request its index and have the
  // remote bound to it.
  // Client should always check if the receiver end is bound before using the
  // index.
  void GetIndex(IndexId index_id,
                Backend backend,
                mojo::PendingReceiver<mojom::Index> index_receiver);

 private:
  friend class LocalSearchServiceProxyTest;

  // GetService will lazily create a LocalSearchService.
  mojom::LocalSearchService* GetService();

  std::unique_ptr<LocalSearchServiceProvider> local_search_service_provider_;
  mojo::Remote<mojom::LocalSearchService> service_;
  std::unique_ptr<SearchMetricsReporter> reporter_;
};

}  // namespace ash::local_search_service

#endif  // CHROMEOS_ASH_COMPONENTS_LOCAL_SEARCH_SERVICE_PUBLIC_CPP_LOCAL_SEARCH_SERVICE_PROXY_H_
