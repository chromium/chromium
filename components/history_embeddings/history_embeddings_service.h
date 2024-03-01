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
#include "components/history_embeddings/sql_database.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/render_frame_host.h"
#include "url/gurl.h"

namespace history_embeddings {

struct UrlPassages {
  UrlPassages();
  ~UrlPassages();
  UrlPassages(const UrlPassages&) = delete;
  UrlPassages& operator=(const UrlPassages&) = delete;
  UrlPassages(UrlPassages&&);
  UrlPassages& operator=(UrlPassages&&);

  GURL url;
  std::vector<std::string> passages;
};

using PassagesCallback = base::OnceCallback<void(UrlPassages)>;

class HistoryEmbeddingsService : public KeyedService {
 public:
  // `storage_dir` will generally be the Profile directory.
  explicit HistoryEmbeddingsService(const base::FilePath& storage_dir);
  HistoryEmbeddingsService(const HistoryEmbeddingsService&) = delete;
  HistoryEmbeddingsService& operator=(const HistoryEmbeddingsService&) = delete;
  ~HistoryEmbeddingsService() override;

  // Initiate async passage extraction from given host's main frame.
  // When extraction completes, the passages will be stored in the database
  // and then given to the callback.
  void RetrievePassages(content::RenderFrameHost& host,
                        PassagesCallback callback);

  // KeyedService:
  void Shutdown() override;

 private:
  // Called indirectly via RetrievePassages when passage extraction completes.
  void OnPassagesRetrieved(PassagesCallback callback, UrlPassages passages);

  // The underlying SQL database, bound to a separate storage sequence.
  // This will be null if the feature flag is disabled.
  base::SequenceBound<SqlDatabase> database_;

  base::WeakPtrFactory<HistoryEmbeddingsService> weak_ptr_factory_;
};

}  // namespace history_embeddings

#endif  // COMPONENTS_HISTORY_EMBEDDINGS_HISTORY_EMBEDDINGS_SERVICE_H_
