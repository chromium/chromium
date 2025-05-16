// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/actor/action_result.h"

#include "base/strings/strcat.h"
#include "base/strings/to_string.h"
#include "base/types/cxx23_to_underlying.h"

namespace actor {

bool IsOk(const mojom::ActionResult& result) {
  return result.code == mojom::ActionResultCode::kOk;
}

mojom::ActionResultPtr MakeOkResult() {
  return MakeResult(mojom::ActionResultCode::kOk);
}

mojom::ActionResultPtr MakeErrorResult() {
  return MakeResult(mojom::ActionResultCode::kError);
}

mojom::ActionResultPtr MakeResult(mojom::ActionResultCode code,
                                  std::string_view msg) {
  return mojom::ActionResult::New(code, std::string(msg));
}

std::string ToDebugString(const mojom::ActionResult& result) {
  if (IsOk(result)) {
    return "ActionResult[OK]";
  } else if (result.message.empty()) {
    return base::StrCat({"ActionResult[", base::ToString(result.code), "]"});
  } else {
    return base::StrCat({"ActionResult[", base::ToString(result.code), ": \"",
                         result.message, "\"]"});
  }
}

}  // namespace actor
