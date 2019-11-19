// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/session/arc_stop_reason.h"

namespace arc {

std::ostream& operator<<(std::ostream& os, ArcStopReason reason) {
  switch (reason) {
#define CASE_IMPL(val)     \
  case ArcStopReason::val: \
    return os << #val

    CASE_IMPL(SHUTDOWN);
    CASE_IMPL(GENERIC_BOOT_FAILURE);
    CASE_IMPL(LOW_DISK_SPACE);
    CASE_IMPL(CRASH);
#undef CASE_IMPL
  }

  // In case of unexpected value, output the int value.
  return os << "ArcStopReason(" << static_cast<int>(reason) << ")";
}

}  // namespace arc
