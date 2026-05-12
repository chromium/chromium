// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_EMBEDDINGS_CANDIDATE_GENERATOR_H_
#define COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_EMBEDDINGS_CANDIDATE_GENERATOR_H_

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/page_content_annotations/content/page_content_extraction_service.h"
#include "components/page_content_annotations/content/page_embeddings_service.h"

namespace page_content_annotations {

// Generates candidates for embeddings from `PageContent`. Will generate at most
// `page_content_passages_to_generate` for the page content passage type.
std::vector<std::pair<std::string, EmbeddingPassageType>>
GenerateEmbeddingsCandidates(const PageContent& page_content,
                             size_t page_content_passages_to_generate);

}  // namespace page_content_annotations

#endif  // COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_EMBEDDINGS_CANDIDATE_GENERATOR_H_
