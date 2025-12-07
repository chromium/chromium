// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_embeddings/mock_history_embeddings_service.h"

namespace history_embeddings {

MockHistoryEmbeddingsService::MockHistoryEmbeddingsService(
    os_crypt_async::OSCryptAsync* os_crypt_async,
    history::HistoryService* history_service,
    passage_embeddings::EmbedderMetadataProvider* embedder_metadata_provider,
    passage_embeddings::Embedder* embedder)
    : HistoryEmbeddingsService(os_crypt_async,
                               history_service,
                               /*page_content_annotations_service=*/nullptr,
                               /*optimization_guide_decider=*/nullptr,
                               embedder_metadata_provider,
                               embedder,
                               /*answerer=*/nullptr,
                               /*intent_classifier=*/nullptr) {}

MockHistoryEmbeddingsService::~MockHistoryEmbeddingsService() = default;

}  // namespace history_embeddings
