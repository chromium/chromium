// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_ACTOR_ACTION_RESULT_H_
#define CHROME_COMMON_ACTOR_ACTION_RESULT_H_

#include <string>
#include <string_view>

#include "chrome/common/actor.mojom.h"

namespace actor {

bool IsOk(const mojom::ActionResult& result);

mojom::ActionResultPtr MakeOkResult();

// TODO(crbug.com/409558980): Replace generic errors with tool-specific ones,
// and remove this function.
mojom::ActionResultPtr MakeErrorResult();

mojom::ActionResultPtr MakeResult(mojom::ActionResultCode code,
                                  std::string_view msg = std::string_view());

std::string ToDebugString(const mojom::ActionResult& result);

}  // namespace actor

#endif  // CHROME_COMMON_ACTOR_ACTION_RESULT_H_
