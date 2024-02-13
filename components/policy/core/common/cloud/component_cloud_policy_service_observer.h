// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_COMPONENT_CLOUD_POLICY_SERVICE_OBSERVER_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_COMPONENT_CLOUD_POLICY_SERVICE_OBSERVER_H_

#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/values_util.h"

namespace policy {

class ComponentCloudPolicyService;

// Callbacks for policy store events used by ComponentCloudPolicyService.
class POLICY_EXPORT ComponentCloudPolicyServiceObserver
    : public base::CheckedObserver {
 public:
  ~ComponentCloudPolicyServiceObserver() override = default;

  // Called on changes to store->policy(). The
  // values in the `component_policy` map are the JSON data received from the
  // server.
  virtual void OnComponentPolicyUpdated(
      const ComponentPolicyMap& component_policy) = 0;
  virtual void OnComponentPolicyServiceDestruction(
      ComponentCloudPolicyService* service) = 0;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_COMPONENT_CLOUD_POLICY_SERVICE_OBSERVER_H_
