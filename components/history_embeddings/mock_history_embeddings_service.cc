// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_embeddings/mock_history_embeddings_service.h"

namespace history_embeddings {

MockHistoryEmbeddingsService::MockHistoryEmbeddingsService(
    history::HistoryService* history_service)
    : HistoryEmbeddingsService(nullptr,
                               history_service,
                               nullptr,
                               nullptr,
                               nullptr,
                               nullptr,
                               nullptr) {}

MockHistoryEmbeddingsService::~MockHistoryEmbeddingsService() = default;

}  // namespace history_embeddings
