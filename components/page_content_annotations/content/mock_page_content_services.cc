// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/content/mock_page_content_services.h"

#include "base/memory/scoped_refptr.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/passage_embeddings/core/passage_embeddings_types.h"

namespace page_content_annotations {

std::optional<ExtractedPageContentResult> CreateExtractionResult(
    std::string text_content,
    bool eligible) {
  scoped_refptr<RefCountedAnnotatedPageContent> page_content =
      base::MakeRefCounted<RefCountedAnnotatedPageContent>();
  optimization_guide::proto::ContentNode* root_node =
      page_content->data.mutable_root_node();
  optimization_guide::proto::ContentAttributes* attributes =
      root_node->mutable_content_attributes();
  attributes->set_attribute_type(
      optimization_guide::proto::CONTENT_ATTRIBUTE_TEXT);
  attributes->mutable_text_data()->set_text_content(text_content);

  ExtractedPageContentResult result;
  result.page_content = std::move(page_content);
  result.is_eligible_for_server_upload = eligible;
  return result;
}

std::vector<PassageEmbedding> CreatePassageEmbeddings(std::string text) {
  std::vector<PassageEmbedding> embeddings;
  embeddings.emplace_back(std::make_pair(text, kPageContent),
                          passage_embeddings::Embedding({1.0f, 0.0f, 0.0f}));
  return embeddings;
}

MockPageContentExtractionService::MockPageContentExtractionService()
    : PageContentExtractionService(nullptr, base::FilePath(), nullptr) {}

MockPageContentExtractionService::~MockPageContentExtractionService() = default;

MockPageEmbeddingsService::MockPageEmbeddingsService(
    PageContentExtractionService* service)
    : PageEmbeddingsService(service) {}

MockPageEmbeddingsService::~MockPageEmbeddingsService() = default;

}  // namespace page_content_annotations
