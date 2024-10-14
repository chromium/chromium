// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_embeddings/answerer.h"

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "components/history_embeddings/history_embeddings_features.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"

namespace history_embeddings {

AnswererResult::AnswererResult() = default;
AnswererResult::AnswererResult(
    ComputeAnswerStatus status,
    std::string query,
    optimization_guide::proto::Answer answer,
    std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry,
    std::string url,
    std::vector<std::string> text_directives)
    : status(status),
      query(std::move(query)),
      answer(std::move(answer)),
      log_entry(std::move(log_entry)),
      url(std::move(url)),
      text_directives(std::move(text_directives)) {}
AnswererResult::AnswererResult(ComputeAnswerStatus status,
                               std::string query,
                               optimization_guide::proto::Answer answer)
    : status(status), query(std::move(query)), answer(std::move(answer)) {}
AnswererResult::AnswererResult(AnswererResult&&) = default;
AnswererResult::~AnswererResult() {
  if (log_entry) {
    optimization_guide::ModelQualityLogEntry::Drop(std::move(log_entry));
  }
}
AnswererResult& AnswererResult::operator=(AnswererResult&&) = default;

void AnswererResult::PopulateScrollToTextFragment(
    const std::vector<std::string>& passages) {
  if (!kScrollTagsEnabled.Get()) {
    return;
  }
  for (int citation_index = 0; citation_index < answer.citations_size();
       citation_index++) {
    int passage_id = 0;
    if (base::StringToInt(answer.citations(citation_index).passage_id(),
                          &passage_id)) {
      // These text IDs are 1-based indices starting with "0001".
      size_t id = static_cast<size_t>(passage_id) - 1;
      if (id >= 0 && id < passages.size()) {
        std::string_view passage = passages[id];
        size_t first = passage.find_first_of(' ');
        size_t last = passage.find_last_of(' ');
        size_t second = std::string::npos;
        size_t second_last = std::string::npos;
        if (first != std::string::npos && last != std::string::npos &&
            first < last) {
          second = passage.find_first_of(' ', first + 1);
          second_last = passage.find_last_of(' ', last - 1);
        }
        if (second != std::string::npos && second_last != std::string::npos &&
            second < second_last) {
          // Include first and last words of passage, separated by a comma.
          text_directives.push_back(
              base::StrCat({passage.substr(0, second), ",",
                            passage.substr(second_last + 1)}));
        } else {
          // Not enough spaces; include full passage.
          text_directives.emplace_back(passage);
        }
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////

Answerer::Context::Context(std::string session_id)
    : session_id(std::move(session_id)) {}

Answerer::Context::Context(const Context& other) = default;
Answerer::Context::Context(Context&& other) = default;
Answerer::Context::~Context() = default;

////////////////////////////////////////////////////////////////////////////////

}  // namespace history_embeddings
