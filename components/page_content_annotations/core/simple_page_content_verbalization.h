// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_SIMPLE_PAGE_CONTENT_VERBALIZATION_H_
#define COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_SIMPLE_PAGE_CONTENT_VERBALIZATION_H_

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "components/optimization_guide/proto/features/common_quality_data.pb.h"

namespace page_content_annotations {

// Returns the number of words in `s` separated by whitespace.
size_t CountWords(std::string_view s);

// Recursively collects plain text from an AnnotatedPageContent ContentNode
// tree into `text`.
void CollectTextForContentNodesRecursively(
    const optimization_guide::proto::ContentNode& node,
    std::vector<std::string>& text);

// Provide a translation of APC to passages. This translation is extremely
// simple and not intended to be a full fidelity representation.
std::vector<std::string> CreatePassagesFromAnnotatedPageContent(
    const optimization_guide::proto::AnnotatedPageContent&
        annotated_page_content,
    size_t max_passages_per_page);

// Provide a translation of PDF text to passages. Similar to
// `CreatePassagesFromAnnotatedPageContent` except it does not enforce a
// minimum number of words per passage.
std::vector<std::string> CreatePassagesFromPDFText(
    const std::string& pdf_text,
    size_t max_passages_per_page);

// Splits `text` into passages of at most `max_words_per_aggregate_passage`,
// enforcing optional `min_words_per_passage` and capping at `max_passages`.
std::vector<std::string> CreatePassagesFromText(
    const std::string& text,
    size_t max_words_per_aggregate_passage,
    size_t min_words_per_passage = 0,
    size_t max_passages = 10);

}  // namespace page_content_annotations

#endif  // COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_SIMPLE_PAGE_CONTENT_VERBALIZATION_H_
