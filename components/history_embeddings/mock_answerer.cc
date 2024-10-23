// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_embeddings/mock_answerer.h"

#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/history_embeddings/history_embeddings_features.h"
#include "components/optimization_guide/proto/features/history_answer.pb.h"

namespace history_embeddings {

MockAnswerer::MockAnswerer() = default;
MockAnswerer::~MockAnswerer() = default;

int64_t MockAnswerer::GetModelVersion() {
  return 1;
}

void MockAnswerer::ComputeAnswer(std::string query,
                                 Context context,
                                 ComputeAnswerCallback callback) {
  optimization_guide::proto::Answer answer;
  answer.set_text(std::string("This is the answer to query '") + query +
                  std::string("'."));
  AnswererResult result(
      static_cast<ComputeAnswerStatus>(kMockAnswererStatus.Get()), query,
      answer, {}, "url.com", {});

  // Set URL and passage citation if available.
  auto it = context.url_passages_map.begin();
  if (it != context.url_passages_map.end()) {
    optimization_guide::proto::Citation* citation =
        result.answer.add_citations();
    citation->set_passage_id("0001");
    result.url = it->first;
    result.PopulateScrollToTextFragment(it->second);
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(result)),
      base::Milliseconds(kMockAnswererDelayMS.Get()));
}

}  // namespace history_embeddings
