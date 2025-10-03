// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/actor/action_result.h"

#include "base/strings/stringprintf.h"
#include "base/strings/to_string.h"
#include "base/time/time.h"
#include "base/types/cxx23_to_underlying.h"

namespace actor {

ActionResultWithLatencyInfo::ActionResultWithLatencyInfo(
    base::TimeTicks start_time,
    base::TimeTicks end_time,
    mojom::ActionResultPtr result)
    : start_time(start_time), end_time(end_time), result(std::move(result)) {}
ActionResultWithLatencyInfo::ActionResultWithLatencyInfo(
    const ActionResultWithLatencyInfo& other)
    : start_time(other.start_time),
      end_time(other.end_time),
      result(other.result.Clone()) {}
ActionResultWithLatencyInfo::~ActionResultWithLatencyInfo() = default;
ActionResultWithLatencyInfo::ActionResultWithLatencyInfo(
    ActionResultWithLatencyInfo&&) = default;

bool IsOk(const mojom::ActionResult& result) {
  return IsOk(result.code);
}

bool IsOk(mojom::ActionResultCode code) {
  return code == mojom::ActionResultCode::kOk;
}

bool RequiresPageStabilization(const mojom::ActionResult& result) {
  return result.requires_page_stabilization;
}

mojom::ActionResultPtr MakeOkResult() {
  return MakeResult(mojom::ActionResultCode::kOk, true);
}

mojom::ActionResultPtr MakeErrorResult() {
  return MakeResult(mojom::ActionResultCode::kError);
}

mojom::ActionResultPtr MakeResult(mojom::ActionResultCode code,
                                  bool requires_page_stabilization,
                                  std::string_view msg) {
  return mojom::ActionResult::New(
      code, requires_page_stabilization, std::string(msg), std::nullopt,
      /*execution_end_time=*/base::TimeTicks::Now());
}

std::string ToDebugString(const mojom::ActionResult& result) {
  if (IsOk(result)) {
    return "ActionResult[OK]";
  } else if (result.message.empty()) {
    return base::StringPrintf(
        "ActionResult[%s][Stability:%s]", base::ToString(result.code),
        base::ToString(result.requires_page_stabilization));
  } else {
    return base::StringPrintf(
        "ActionResult[%s][Stability:%s]: %s", base::ToString(result.code),
        base::ToString(result.requires_page_stabilization), result.message);
  }
}

}  // namespace actor
