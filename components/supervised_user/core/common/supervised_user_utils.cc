// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/common/supervised_user_utils.h"

#include "base/notreached.h"

namespace supervised_user {

std::string FilteringBehaviorReasonToString(FilteringBehaviorReason reason) {
  switch (reason) {
    case FilteringBehaviorReason::DEFAULT:
      return "Default";
    case FilteringBehaviorReason::ASYNC_CHECKER:
      return "AsyncChecker";
    case FilteringBehaviorReason::DENYLIST:
      return "Denylist";
    case FilteringBehaviorReason::MANUAL:
      return "Manual";
    case FilteringBehaviorReason::ALLOWLIST:
      return "Allowlist";
    case FilteringBehaviorReason::NOT_SIGNED_IN:
      return "NotSignedIn";
  }
  return "Unknown";
}

}  // namespace supervised_user
