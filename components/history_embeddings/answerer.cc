// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_embeddings/answerer.h"

namespace history_embeddings {

AnswererResult::AnswererResult() = default;
AnswererResult::AnswererResult(ComputeAnswerStatus status,
                               std::string query,
                               optimization_guide::proto::Answer answer,
                               std::string url,
                               std::vector<std::string> text_directives)
    : status(status),
      query(std::move(query)),
      answer(std::move(answer)),
      url(std::move(url)),
      text_directives(std::move(text_directives)) {}
AnswererResult::AnswererResult(ComputeAnswerStatus status,
                               std::string query,
                               optimization_guide::proto::Answer answer)
    : status(status), query(std::move(query)), answer(std::move(answer)) {}
AnswererResult::AnswererResult(const AnswererResult&) = default;
AnswererResult::~AnswererResult() = default;

Answerer::Context::Context() = default;

Answerer::Context::Context(const Context& other)
    : url_passages_map(other.url_passages_map) {}

Answerer::Context::~Context() = default;

}  // namespace history_embeddings
