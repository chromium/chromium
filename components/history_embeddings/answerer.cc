// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_embeddings/answerer.h"

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

////////////////////////////////////////////////////////////////////////////////

Answerer::Context::Context(std::string session_id)
    : session_id(std::move(session_id)) {}

Answerer::Context::Context(const Context& other) = default;
Answerer::Context::Context(Context&& other) = default;
Answerer::Context::~Context() = default;

}  // namespace history_embeddings
