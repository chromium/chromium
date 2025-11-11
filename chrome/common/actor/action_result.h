// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_ACTOR_ACTION_RESULT_H_
#define CHROME_COMMON_ACTOR_ACTION_RESULT_H_

#include <string>
#include <string_view>

#include "chrome/common/actor.mojom.h"

namespace actor {

// The result of each action with latency information.
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

// TODO(crbug.com/409558980): Replace generic errors with tool-specific ones,
// and remove this function.
mojom::ActionResultPtr MakeErrorResult();

// TODO(b/459615712): Rename this to MakeErrorResult to make it clear that this
// shouldn't be used for successful results as the default page_stabilization
// argument makes this error-prone. This is runtime asserted inside.
mojom::ActionResultPtr MakeResult(mojom::ActionResultCode code,
                                  bool requires_page_stabilization = false,
                                  std::string_view msg = std::string_view());

std::string ToDebugString(const mojom::ActionResult& result);

}  // namespace actor

#endif  // CHROME_COMMON_ACTOR_ACTION_RESULT_H_
