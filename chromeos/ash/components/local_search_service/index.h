// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOCAL_SEARCH_SERVICE_INDEX_H_
#define CHROMEOS_ASH_COMPONENTS_LOCAL_SEARCH_SERVICE_INDEX_H_

#include <cstdint>
#include <string>
#include <vector>

#include "chromeos/ash/components/local_search_service/public/mojom/index.mojom.h"
#include "chromeos/ash/components/local_search_service/public/mojom/local_search_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace ash::local_search_service {

class Index : public mojom::Index {
 public:
  explicit Index(IndexId index_id, Backend backend);
  ~Index() override;

  void BindReceiver(mojo::PendingReceiver<mojom::Index> receiver);

  // Call once to set the SearchMetricsReporter remote.
  void SetReporterRemote(
      mojo::PendingRemote<mojom::SearchMetricsReporter> reporter_remote);

  // mojom::Index
  void SetSearchParams(const SearchParams& search_params,
                       SetSearchParamsCallback callback) override;

  void SetSearchParams(const SearchParams& search_params) {
    search_params_ = search_params;
  }

  SearchParams GetSearchParamsForTesting() const { return search_params_; }

 protected:
  // Get size of the index for metrics. Only accurate if called after index has
  // done updating.
  virtual uint32_t GetIndexSize() const = 0;

  // Logs daily search metrics if |reporter_remote_| is bound. Also logs
  // other UMA metrics (number results and search latency).
  void MaybeLogSearchResultsStats(ResponseStatus status,
                                  size_t num_results,
                                  base::TimeDelta latency);

  // Logs number of documents in the index.
  void MaybeLogIndexSize();

  void AddOrUpdateCallbackWithTime(AddOrUpdateCallback callback,
                                   const base::Time start_time);
  void DeleteCallbackWithTime(DeleteCallback callback,
                              const base::Time start_time,
                              uint32_t num_deleted);
  void UpdateDocumentsCallbackWithTime(UpdateDocumentsCallback callback,
                                       const base::Time start_time,
                                       uint32_t num_deleted);
  void ClearIndexCallbackWithTime(ClearIndexCallback callback,
                                  const base::Time start_time);

  SearchParams search_params_;
  IndexId index_id_;

 private:
  std::string histogram_prefix_;
  mojo::Remote<mojom::SearchMetricsReporter> reporter_remote_;
  mojo::ReceiverSet<mojom::Index> receivers_;
};

}  // namespace ash::local_search_service

#endif  // CHROMEOS_ASH_COMPONENTS_LOCAL_SEARCH_SERVICE_INDEX_H_
