// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/ranking/answer_ranker.h"

namespace app_list {

AnswerRanker::AnswerRanker() = default;
AnswerRanker::~AnswerRanker() = default;

void AnswerRanker::Start(const std::u16string& query,
                         ResultsMap& results,
                         CategoriesList& categories) {
  // TODO(crbug.com/1275408): WIP.
}

void AnswerRanker::UpdateResultRanks(ResultsMap& results,
                                     ProviderType provider) {
  // TODO(crbug.com/1275408): WIP.
}

}  // namespace app_list
