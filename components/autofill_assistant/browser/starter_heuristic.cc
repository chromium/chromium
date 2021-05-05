// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/starter_heuristic.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "third_party/re2/src/re2/re2.h"

namespace autofill_assistant {

const char kStubCartHeuristic[] = "cart";

StarterHeuristic::StarterHeuristic() = default;
StarterHeuristic::~StarterHeuristic() = default;

bool StarterHeuristic::IsHeuristicMatch(const GURL& url) const {
  const re2::RE2 re(kStubCartHeuristic);
  // TODO(arbesser): implement the heuristic.
  return re2::RE2::PartialMatch(url.spec(), re);
}

void StarterHeuristic::RunHeuristicAsync(
    const GURL& url,
    base::OnceCallback<void(bool result)> callback) const {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&StarterHeuristic::IsHeuristicMatch,
                     base::RetainedRef(this), url),
      std::move(callback));
}

}  // namespace autofill_assistant
