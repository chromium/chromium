// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/content/embeddings_candidate_generator.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "base/feature_list.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "components/page_content_annotations/content/page_content_extraction_service.h"
#include "components/page_content_annotations/core/simple_page_content_verbalization.h"
#include "components/passage_embeddings/core/passage_embeddings_features.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace page_content_annotations {

std::vector<std::pair<std::string, EmbeddingPassageType>>
GenerateEmbeddingsCandidates(const PageContent& page_content,
                             size_t page_content_passages_to_generate,
                             const std::string& title,
                             const std::string& url) {
  if (IsPDFTextPtr(page_content) &&
      !base::FeatureList::IsEnabled(
          passage_embeddings::kPDFEmbeddingsGeneration)) {
    return {};
  }

  if (!IsPageContentValid(page_content)) {
    return {};
  }

  std::vector<std::pair<std::string, EmbeddingPassageType>> candidates;

  // Create passages from page content, which can be either an
  // AnnotatedPageContent or the extracted text from PDF. Note
  // `IsPageContentValid` has already checked the pointer against nullptr, so it
  // does not need to be checked again below.
  std::vector<std::string> passages = std::visit(
      absl::Overload{
          [page_content_passages_to_generate](
              RefCountedAnnotatedPageContentPtr apc_ptr) {
            return CreatePassagesFromAnnotatedPageContent(
                apc_ptr->data, page_content_passages_to_generate);
          },
          [page_content_passages_to_generate](
              RefCountedPDFTextPtr pdf_text_ptr) {
            return CreatePassagesFromPDFText(pdf_text_ptr->data,
                                             page_content_passages_to_generate);
          },
      },
      page_content);

  // Add passages to candidates.
  for (const auto& passage : passages) {
    candidates.emplace_back(passage, EmbeddingPassageType::kPageContent);
  }

  // TODO(b/504577535): Once PDF bookmark extraction is supported, include the
  // bookmark in embeddings candidates.
  // TODO(b/504577256): Once PDF accessibility info extraction is supported,
  // include the bookmark in embeddings candidates.
  // Add candidates using the title and URL.
  if (!title.empty()) {
    candidates.emplace_back(title, EmbeddingPassageType::kTitle);
  }
  if (!title.empty() && !url.empty()) {
    candidates.emplace_back(base::StrCat({title, " - ", url}),
                            EmbeddingPassageType::kTitleAndUrl);
  }

  return candidates;
}

}  // namespace page_content_annotations
