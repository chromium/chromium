// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_EMBEDDINGS_MOCK_HISTORY_EMBEDDINGS_SERVICE_H_
#define COMPONENTS_HISTORY_EMBEDDINGS_MOCK_HISTORY_EMBEDDINGS_SERVICE_H_

#include <optional>
#include <string>

#include "base/time/time.h"
#include "components/history/core/browser/history_service.h"
#include "components/history_embeddings/history_embeddings_service.h"
#include "components/passage_embeddings/passage_embeddings_types.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace os_crypt_async {
class OSCryptAsync;
}  // namespace os_crypt_async

namespace history_embeddings {

class MockHistoryEmbeddingsService : public HistoryEmbeddingsService {
 public:
  MOCK_METHOD(SearchResult,
              Search,
              (SearchResult * previous_search_result,
               std::string query,
               std::optional<base::Time> time_range_start,
               size_t count,
               bool skip_answering,
               SearchResultCallback callback),
              (override));
  explicit MockHistoryEmbeddingsService(
      os_crypt_async::OSCryptAsync* os_crypt_async,
      history::HistoryService* history_service,
      passage_embeddings::EmbedderMetadataProvider* embedder_metadata_provider,
      passage_embeddings::Embedder* embedder);
  ~MockHistoryEmbeddingsService() override;
};

}  // namespace history_embeddings

#endif  // COMPONENTS_HISTORY_EMBEDDINGS_MOCK_HISTORY_EMBEDDINGS_SERVICE_H_
