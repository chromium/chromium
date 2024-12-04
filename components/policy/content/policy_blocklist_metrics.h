// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CONTENT_POLICY_BLOCKLIST_METRICS_H_
#define COMPONENTS_POLICY_CONTENT_POLICY_BLOCKLIST_METRICS_H_

#include <optional>

#include "base/supports_user_data.h"
#include "base/time/time.h"

namespace content {
class NavigationHandle;
}  // namespace content

struct PolicyBlocklistMetrics : public base::SupportsUserData::Data {
  static void Create(content::NavigationHandle& handle);
  static PolicyBlocklistMetrics* Get(content::NavigationHandle& handle);

  PolicyBlocklistMetrics(const PolicyBlocklistMetrics&) = delete;
  PolicyBlocklistMetrics& operator=(const PolicyBlocklistMetrics&) = delete;
  ~PolicyBlocklistMetrics() override = default;

  uint32_t redirect_count = 0u;
  base::TimeDelta request_to_response_time;
  base::TimeDelta response_defer_duration;
  std::optional<bool> cache_hit;

 private:
  PolicyBlocklistMetrics() = default;
};

#endif  // COMPONENTS_POLICY_CONTENT_POLICY_BLOCKLIST_METRICS_H_
