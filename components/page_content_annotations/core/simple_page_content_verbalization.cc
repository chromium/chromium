// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/core/simple_page_content_verbalization.h"

#include "base/strings/strcat.h"
#include "base/strings/string_tokenizer.h"
#include "components/passage_embeddings/core/passage_embeddings_features.h"

namespace page_content_annotations {

namespace {

void CollectTextForContentNode(
    const optimization_guide::proto::ContentNode& node,
    std::vector<std::string>& text) {
  if (!node.has_content_attributes()) {
    return;
  }

  const auto& attributes = node.content_attributes();

  switch (attributes.attribute_type()) {
    case optimization_guide::proto::ContentAttributeType::
        CONTENT_ATTRIBUTE_TABLE:
      if (!attributes.table_data().table_name().empty()) {
        text.push_back(attributes.table_data().table_name());
      }
      break;

    case optimization_guide::proto::ContentAttributeType::
        CONTENT_ATTRIBUTE_TEXT:
      if (!attributes.text_data().text_content().empty()) {
        text.push_back(attributes.text_data().text_content());
      }
      break;

    case optimization_guide::proto::ContentAttributeType::
        CONTENT_ATTRIBUTE_IMAGE:
      if (!attributes.image_data().image_caption().empty()) {
        text.push_back(attributes.image_data().image_caption());
      }
      break;

    default:
      break;
  }
}

void AppendWithWhitespaceSeparator(std::string& str,
                                   std::string_view str_to_append) {
  if (str_to_append.empty()) {
    return;
  }

  if (str.empty() || str.back() == ' ') {
    str.append(str_to_append);
    return;
  }

  base::StrAppend(&str, {" ", str_to_append});
}

}  // namespace

size_t CountWords(std::string_view s) {
  if (s.empty()) {
    return 0;
  }
  size_t word_count = (s[0] == ' ') ? 0 : 1;
  for (size_t i = 1; i < s.length(); i++) {
    if (s[i] != ' ' && s[i - 1] == ' ') {
      word_count++;
    }
  }
  return word_count;
}

void CollectTextForContentNodesRecursively(
    const optimization_guide::proto::ContentNode& node,
    std::vector<std::string>& text) {
  CollectTextForContentNode(node, text);

  for (const auto& child : node.children_nodes()) {
    CollectTextForContentNodesRecursively(child, text);
  }
}

// Provide a translation of APC to passages. This translation is extremely
// simple and not intended to be a full fidelity representation.
std::vector<std::string> CreatePassagesFromAnnotatedPageContent(
    const optimization_guide::proto::AnnotatedPageContent&
        annotated_page_content,
    size_t max_passages_per_page) {
  if (!annotated_page_content.has_root_node()) {
    return {};
  }
  std::vector<std::string> text;
  CollectTextForContentNodesRecursively(annotated_page_content.root_node(),
                                        text);

  if (text.empty()) {
    return {};
  }

  const size_t max_words_per_aggregate_passage =
      passage_embeddings::kMaxWordsPerAggregatePassage.Get();
  if (max_words_per_aggregate_passage == 0) {
    return {};
  }

  const size_t min_words_per_passage =
      passage_embeddings::kMinWordsPerPassage.Get();

  std::vector<std::string> passages{""};
  size_t current_passage_words = 0;

  for (const std::string& item : text) {
    base::StringTokenizer tokenizer(item, base::kWhitespaceASCII);
    while (tokenizer.GetNext()) {
      if (current_passage_words >= max_words_per_aggregate_passage) {
        if (passages.size() >= max_passages_per_page) {
          if (!passages.empty() && passages.back().empty()) {
            passages.pop_back();
          }
          if (!passages.empty() &&
              CountWords(passages.back()) < min_words_per_passage) {
            passages.pop_back();
          }
          return passages;
        }
        passages.push_back("");
        current_passage_words = 0;
      }

      AppendWithWhitespaceSeparator(passages.back(), tokenizer.token_piece());
      ++current_passage_words;
    }
  }

  if (!passages.empty() && passages.back().empty()) {
    passages.pop_back();
  }

  if (!passages.empty() &&
      CountWords(passages.back()) < min_words_per_passage) {
    passages.pop_back();
  }

  return passages;
}

// Provide a translation of PDF text to passages. Similar to
// `CreatePassagesFromAnnotatedPageContent` except it does not enforce a
// minimum number of words per passage.
std::vector<std::string> CreatePassagesFromPDFText(
    const std::string& pdf_text,
    size_t max_passages_per_page) {
  if (pdf_text.empty() || max_passages_per_page == 0) {
    return {};
  }

  const size_t max_words_per_aggregate_passage =
      passage_embeddings::kMaxWordsPerAggregatePassage.Get();
  if (max_words_per_aggregate_passage == 0) {
    return {};
  }

  base::StringTokenizer tokenizer(pdf_text, base::kWhitespaceASCII);
  std::vector<std::string> passages{""};
  size_t current_passage_words = 0;

  while (tokenizer.GetNext()) {
    if (current_passage_words >= max_words_per_aggregate_passage) {
      if (passages.size() >= max_passages_per_page) {
        break;
      }
      passages.push_back("");
      current_passage_words = 0;
    }

    AppendWithWhitespaceSeparator(passages.back(), tokenizer.token_piece());
    ++current_passage_words;
  }

  if (!passages.empty() && passages.back().empty()) {
    passages.pop_back();
  }

  return passages;
}

std::vector<std::string> CreatePassagesFromText(
    const std::string& text,
    size_t max_words_per_aggregate_passage,
    size_t min_words_per_passage,
    size_t max_passages) {
  if (text.empty() || max_words_per_aggregate_passage == 0 ||
      max_passages == 0) {
    return {};
  }

  base::StringTokenizer tokenizer(text, base::kWhitespaceASCII);
  std::vector<std::string> passages{""};
  size_t current_passage_words = 0;

  while (tokenizer.GetNext()) {
    if (current_passage_words >= max_words_per_aggregate_passage) {
      if (passages.size() >= max_passages) {
        if (!passages.empty() && passages.back().empty()) {
          passages.pop_back();
        }
        if (!passages.empty() &&
            CountWords(passages.back()) < min_words_per_passage) {
          passages.pop_back();
        }
        return passages;
      }
      passages.push_back("");
      current_passage_words = 0;
    }

    AppendWithWhitespaceSeparator(passages.back(), tokenizer.token_piece());
    ++current_passage_words;
  }

  if (!passages.empty() && passages.back().empty()) {
    passages.pop_back();
  }

  if (!passages.empty() &&
      CountWords(passages.back()) < min_words_per_passage) {
    passages.pop_back();
  }

  return passages;
}

}  // namespace page_content_annotations
