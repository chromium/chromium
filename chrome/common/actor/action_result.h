// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_ACTOR_ACTION_RESULT_H_
#define CHROME_COMMON_ACTOR_ACTION_RESULT_H_

#include <concepts>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

#include "base/time/time.h"
#include "chrome/common/actor.mojom.h"
#include "components/actor/public/mojom/actor_types.mojom-forward.h"

namespace actor {

// The result of each action with latency information.
// TODO(crbug.com/490381613): Create an object to wrap
// std::vector<ActionResultWithLatencyInfo> that can replace several of the free
// functions below.
struct ActionResultWithLatencyInfo {
  base::TimeTicks start_time;
  base::TimeTicks end_time;
  mojom::ActionResultPtr result;

  ActionResultWithLatencyInfo(base::TimeTicks start_time,
                              base::TimeTicks end_time,
                              mojom::ActionResultPtr result);
  ActionResultWithLatencyInfo(ActionResultWithLatencyInfo&&);
  ActionResultWithLatencyInfo(const ActionResultWithLatencyInfo&);
  ActionResultWithLatencyInfo& operator=(const ActionResultWithLatencyInfo&) =
      delete;
  ~ActionResultWithLatencyInfo();
};

bool IsOk(const mojom::ActionResult& result);

bool IsOk(mojom::ActionResultCode code);

bool RequiresPageStabilization(const mojom::ActionResult& result);

mojom::ActionResultPtr MakeOkResult(bool requires_page_stabilization = true);

// TODO(b/459615712): Rename this to MakeErrorResult to make it clear that this
// shouldn't be used for successful results as the default page_stabilization
// argument makes this error-prone. This is runtime asserted inside.
mojom::ActionResultPtr MakeResult(mojom::ActionResultCode code,
                                  bool requires_page_stabilization = false,
                                  std::string_view msg = std::string_view());

// Returns a singleton result vector using the given `result`.
std::vector<ActionResultWithLatencyInfo> MakeResultVector(
    mojom::ActionResultPtr result);

// Returns a singleton result vector using the given `code`.
std::vector<ActionResultWithLatencyInfo> MakeResultVector(
    mojom::ActionResultCode code);

// Assigns `error_code` and `index_of_failed_action` to the the first member of
// `results` with a non-Ok result, if any.
template <std::ranges::random_access_range R>
  requires std::same_as<std::ranges::range_value_t<R>,
                        ActionResultWithLatencyInfo>
void ExtractErrorResult(const R& results,
                        mojom::ActionResultCode* error_code,
                        std::optional<size_t>& index_of_failed_action) {
  for (size_t i = 0; i < results.size(); ++i) {
    if (!IsOk(results[i].result->code)) {
      *error_code = results[i].result->code;
      index_of_failed_action = i;
      return;
    }
  }
}

std::string ToDebugString(const mojom::ActionResult& result);

}  // namespace actor

#endif  // CHROME_COMMON_ACTOR_ACTION_RESULT_H_
