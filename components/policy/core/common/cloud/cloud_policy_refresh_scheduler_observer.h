// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_REFRESH_SCHEDULER_OBSERVER_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_REFRESH_SCHEDULER_OBSERVER_H_

#include "base/observer_list_types.h"
#include "components/policy/policy_export.h"

namespace policy {

class CloudPolicyRefreshScheduler;

// Callbacks for policy refresh scheduler events. Unlike most other observers in
// this directory, this class is defined as non-nested to allow it being used in
// other external classes without pulling too many other dependencies. Please
// see comments on https://crrev.com/c/3536708 for more context.
class POLICY_EXPORT CloudPolicyRefreshSchedulerObserver
    : public base::CheckedObserver {
 public:
  // Called before each attempt to fetch policy.
  virtual void OnFetchAttempt(CloudPolicyRefreshScheduler* scheduler) = 0;

  // Called upon refresh scheduler destruction.
  virtual void OnRefreshSchedulerDestruction(
      CloudPolicyRefreshScheduler* scheduler) = 0;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_REFRESH_SCHEDULER_OBSERVER_H_
