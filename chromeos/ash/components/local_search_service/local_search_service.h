// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOCAL_SEARCH_SERVICE_LOCAL_SEARCH_SERVICE_H_
#define CHROMEOS_ASH_COMPONENTS_LOCAL_SEARCH_SERVICE_LOCAL_SEARCH_SERVICE_H_

#include "chromeos/ash/components/local_search_service/index.h"
#include "chromeos/ash/components/local_search_service/public/mojom/index.mojom.h"
#include "chromeos/ash/components/local_search_service/public/mojom/local_search_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace ash::local_search_service {

class LocalSearchService : public mojom::LocalSearchService {
 public:
  explicit LocalSearchService(
      mojo::PendingReceiver<mojom::LocalSearchService> receiver);
  ~LocalSearchService() override;

  // mojom::LocalSearchService:
  void BindIndex(
      IndexId index_id,
      Backend backend,
      mojo::PendingReceiver<mojom::Index> index_receiver,
      mojo::PendingRemote<mojom::SearchMetricsReporter> reporter_remote,
      LocalSearchService::BindIndexCallback callback) override;

 private:
  mojo::Receiver<mojom::LocalSearchService> receiver_;
  std::map<IndexId, std::unique_ptr<Index>> indices_;
};

}  // namespace ash::local_search_service

#endif  // CHROMEOS_ASH_COMPONENTS_LOCAL_SEARCH_SERVICE_LOCAL_SEARCH_SERVICE_H_
