// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_MOCK_PAGE_CONTENT_SERVICES_H_
#define COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_MOCK_PAGE_CONTENT_SERVICES_H_

#include <optional>
#include <string>
#include <vector>

#include "components/page_content_annotations/content/page_content_extraction_service.h"
#include "components/page_content_annotations/content/page_embeddings_service.h"
#include "components/page_content_annotations/core/page_content_extraction_types.h"
#include "components/page_content_annotations/core/page_embeddings_common.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {
class Page;
}

namespace page_content_annotations {

std::optional<ExtractedPageContentResult> CreateExtractionResult(
    std::string text_content,
    bool eligible);

std::vector<PassageEmbedding> CreatePassageEmbeddings(std::string text);

class MockPageContentExtractionService : public PageContentExtractionService {
 public:
  MockPageContentExtractionService();
  ~MockPageContentExtractionService() override;

  MOCK_METHOD(std::optional<ExtractedPageContentResult>,
              GetExtractedPageContentAndEligibilityForPage,
              (content::Page&),
              (override));
};

class MockPageEmbeddingsService : public PageEmbeddingsService {
 public:
  explicit MockPageEmbeddingsService(PageContentExtractionService* service);
  ~MockPageEmbeddingsService() override;

  MOCK_METHOD(std::vector<PassageEmbedding>,
              GetEmbeddings,
              (content::Page&),
              (const, override));
};

}  // namespace page_content_annotations

#endif  // COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CONTENT_MOCK_PAGE_CONTENT_SERVICES_H_
