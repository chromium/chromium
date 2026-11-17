// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_ACTOR_ACTION_RESULT_H_
#define CHROME_COMMON_ACTOR_ACTION_RESULT_H_

#include <string>
#include <string_view>

#include "chrome/common/actor.mojom.h"
#include "components/actor/public/mojom/actor_types.mojom-forward.h"

namespace actor {

bool IsOk(const mojom::ActionResult& result);

bool IsOk(mojom::ActionResultCode code);

bool RequiresPageStabilization(const mojom::ActionResult& result);

mojom::ActionResultPtr MakeOkResult(bool requires_page_stabilization = true);
mojom::ActionResultPtr MakeOkResultWithMessage(bool requires_page_stabilization,
                                               const std::string& message);

// TODO(b/459615712): Rename this to MakeErrorResult to make it clear that this
// shouldn't be used for successful results as the default page_stabilization
// argument makes this error-prone. This is runtime asserted inside.
mojom::ActionResultPtr MakeResult(mojom::ActionResultCode code,
                                  bool requires_page_stabilization = false,
                                  std::string_view msg = std::string_view());

std::string ToDebugString(const mojom::ActionResult& result);

}  // namespace actor

#endif  // CHROME_COMMON_ACTOR_ACTION_RESULT_H_
