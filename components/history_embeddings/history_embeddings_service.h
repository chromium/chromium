// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_EMBEDDINGS_HISTORY_EMBEDDINGS_SERVICE_H_
#define COMPONENTS_HISTORY_EMBEDDINGS_HISTORY_EMBEDDINGS_SERVICE_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequence_bound.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/history_embeddings/sql_database.h"
#include "components/history_embeddings/vector_database.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/render_frame_host.h"

namespace history_embeddings {

using PassagesCallback = base::OnceCallback<void(UrlPassages)>;
using ComputeEmbeddingsCallback =
    base::OnceCallback<std::vector<Embedding>(const UrlPassages&)>;
using SearchResultCallback = base::OnceCallback<void(std::vector<ScoredUrl>)>;

class HistoryEmbeddingsService : public KeyedService,
                                 public history::HistoryServiceObserver {
 public:
  // `storage_dir` will generally be the Profile directory.
  // `history_service` is never nullptr.
  HistoryEmbeddingsService(const base::FilePath& storage_dir,
                           history::HistoryService* history_service);
  HistoryEmbeddingsService(const HistoryEmbeddingsService&) = delete;
  HistoryEmbeddingsService& operator=(const HistoryEmbeddingsService&) = delete;
  ~HistoryEmbeddingsService() override;

  // Initiate async passage extraction from given host's main frame.
  // When extraction completes, the passages will be stored in the database
  // and then given to the callback.
  void RetrievePassages(content::RenderFrameHost& host,
                        PassagesCallback callback);

  // Find top `count` URL visit info entries nearest given `query`. Pass
  // results to given `callback` when search completes.
  void Search(std::string query, size_t count, SearchResultCallback callback);

  // KeyedService:
  void Shutdown() override;

  // history::HistoryServiceObserver:
  void OnURLsDeleted(history::HistoryService* history_service,
                     const history::DeletionInfo& deletion_info) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(HistoryEmbeddingsTest,
                           ConstructsAndComputesEmbeddings);

  // A utility container to wrap anything that should be accessed on
  // the separate storage worker sequence.
  struct Storage {
    explicit Storage(const base::FilePath& storage_dir);

    // Called on the worker sequence to process and persist passages &
    // embeddings.
    void ProcessAndStorePassages(ComputeEmbeddingsCallback compute_embeddings,
                                 UrlPassages url_passages);

    // Runs search on worker sequence.
    std::vector<ScoredUrl> Search(std::string query, size_t count);

    // A VectorDatabase implementation that holds data in memory.
    VectorDatabaseInMemory vector_database;

    // The underlying SQL database for persistent storage.
    SqlDatabase sql_database;
  };

  // Called indirectly via RetrievePassages when passage extraction completes.
  void OnPassagesRetrieved(PassagesCallback callback, UrlPassages passages);

  // Tracks the observed history service, for cleanup.
  base::ScopedObservation<history::HistoryService,
                          history::HistoryServiceObserver>
      history_service_observation_{this};

  // Storage is bound to a separate sequence.
  // This will be null if the feature flag is disabled.
  base::SequenceBound<Storage> storage_;

  base::WeakPtrFactory<HistoryEmbeddingsService> weak_ptr_factory_;
};

}  // namespace history_embeddings

#endif  // COMPONENTS_HISTORY_EMBEDDINGS_HISTORY_EMBEDDINGS_SERVICE_H_
