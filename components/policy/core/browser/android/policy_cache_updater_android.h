// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_BROWSER_ANDROID_POLICY_CACHE_UPDATER_ANDROID_H_
#define COMPONENTS_POLICY_CORE_BROWSER_ANDROID_POLICY_CACHE_UPDATER_ANDROID_H_

#include "base/memory/raw_ptr.h"
#include "components/policy/core/browser/configuration_policy_handler_list.h"
#include "components/policy/core/common/policy_service.h"

namespace policy {

class ConfigurationPolicyHandlerList;

namespace android {

class POLICY_EXPORT PolicyCacheUpdater : public PolicyService::Observer {
 public:
  PolicyCacheUpdater(PolicyService* policy_service,
                     const ConfigurationPolicyHandlerList* handler_list);
  PolicyCacheUpdater(const PolicyCacheUpdater&) = delete;
  PolicyCacheUpdater& operator=(const PolicyCacheUpdater&) = delete;
  ~PolicyCacheUpdater() override;

  // PolicyService::Observer
  void OnPolicyUpdated(const PolicyNamespace& ns,
                       const PolicyMap& previous,
                       const PolicyMap& current) override;

 private:
  void UpdateCache(const PolicyMap& current_policy_map);

  raw_ptr<PolicyService> policy_service_;
  raw_ptr<const ConfigurationPolicyHandlerList> handler_list_;
};

}  // namespace android
}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_BROWSER_ANDROID_POLICY_CACHE_UPDATER_ANDROID_H_
