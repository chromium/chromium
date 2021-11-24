// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/power_metrics/resource_coalition_mac.h"

#include <libproc.h>

#include "base/check_op.h"

extern "C" int coalition_info_resource_usage(
    uint64_t cid,
    struct coalition_resource_usage* cru,
    size_t sz);

namespace power_metrics {

absl::optional<uint64_t> GetProcessCoalitionId(base::ProcessId pid) {
  proc_pidcoalitioninfo coalition_info = {};
  int res = proc_pidinfo(pid, PROC_PIDCOALITIONINFO, 0, &coalition_info,
                         sizeof(coalition_info));

  if (res != sizeof(coalition_info))
    return absl::nullopt;

  return coalition_info.coalition_id[COALITION_TYPE_RESOURCE];
}

std::unique_ptr<coalition_resource_usage> GetCoalitionResourceUsage(
    int64_t coalition_id) {
  auto cru = std::make_unique<coalition_resource_usage>();
  uint64_t res = coalition_info_resource_usage(
      coalition_id, cru.get(), sizeof(coalition_resource_usage));
  if (res == 0U)
    return cru;
  return nullptr;
}

}  // power_metrics
