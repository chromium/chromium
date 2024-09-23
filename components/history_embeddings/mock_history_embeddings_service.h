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
#include "testing/gmock/include/gmock/gmock.h"

namespace history_embeddings {

class MockHistoryEmbeddingsService : public HistoryEmbeddingsService {
 public:
  MOCK_METHOD(SearchResult,
              Search,
              (SearchResult * previous_search_result,
               std::string query,
               std::optional<base::Time> time_range_start,
               size_t count,
               SearchResultCallback callback),
              (override));
  explicit MockHistoryEmbeddingsService(
      history::HistoryService* history_service);
  ~MockHistoryEmbeddingsService() override;
};

}  // namespace history_embeddings

#endif  // COMPONENTS_HISTORY_EMBEDDINGS_MOCK_HISTORY_EMBEDDINGS_SERVICE_H_
