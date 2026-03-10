// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/core/page_embeddings_common.h"

namespace page_content_annotations {

PassageEmbedding::PassageEmbedding() = default;
PassageEmbedding::~PassageEmbedding() = default;
PassageEmbedding::PassageEmbedding(const PassageEmbedding& other) = default;
PassageEmbedding::PassageEmbedding(
    std::pair<std::string, EmbeddingPassageType> passage,
    passage_embeddings::Embedding embedding)
    : passage(std::move(passage)), embedding(std::move(embedding)) {}

}  // namespace page_content_annotations
