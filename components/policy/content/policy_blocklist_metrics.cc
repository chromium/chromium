// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/content/policy_blocklist_metrics.h"

#include <memory>

#include "content/public/browser/navigation_handle.h"

namespace {
constexpr char kUserDataKey[] = "PolicyBlocklistMetrics";
}

// static
PolicyBlocklistMetrics* PolicyBlocklistMetrics::Get(
    content::NavigationHandle& handle) {
  return static_cast<PolicyBlocklistMetrics*>(handle.GetUserData(kUserDataKey));
}

// static
void PolicyBlocklistMetrics::Create(content::NavigationHandle& handle) {
  handle.SetUserData(kUserDataKey,
                     base::WrapUnique(new PolicyBlocklistMetrics()));
}
