// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_COMPONENT_CLOUD_POLICY_UPDATER_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_COMPONENT_CLOUD_POLICY_UPDATER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "components/policy/core/common/cloud/external_policy_data_updater.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/policy_export.h"

namespace base {
class SequencedTaskRunner;
}

namespace enterprise_management {
class PolicyFetchResponse;
}

namespace policy {

class ComponentCloudPolicyStore;
class ExternalPolicyDataFetcher;

// This class downloads external policy data, given PolicyFetchResponses.
// It validates the PolicyFetchResponse and its corresponding data, and caches
// them in a ComponentCloudPolicyStore. It also enforces size limits on what's
// cached.
// It retries to download the policy data periodically when a download fails.
class POLICY_EXPORT ComponentCloudPolicyUpdater {
 public:
  // This class runs on the background thread represented by |task_runner|,
  // which must support file I/O. All network I/O is delegated to the
  // |external_policy_data_fetcher|.
  ComponentCloudPolicyUpdater(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      std::unique_ptr<ExternalPolicyDataFetcher> external_policy_data_fetcher,
      ComponentCloudPolicyStore* store);
  ComponentCloudPolicyUpdater(const ComponentCloudPolicyUpdater&) = delete;
  ComponentCloudPolicyUpdater& operator=(const ComponentCloudPolicyUpdater&) =
      delete;
  ~ComponentCloudPolicyUpdater();

  // |response| is the latest policy information fetched for component
  // represented by namespace |ns|.
  // This method schedules the download of the policy data, if |response| is
  // validated. If the downloaded data also passes validation then that data
  // will be passed to the |store_|.
  void UpdateExternalPolicy(
      const PolicyNamespace& ns,
      std::unique_ptr<enterprise_management::PolicyFetchResponse> response);

  // Cancels any pending operations for the given namespace.
  void CancelUpdate(const PolicyNamespace& ns);

 private:
  const raw_ptr<ComponentCloudPolicyStore> store_;
  ExternalPolicyDataUpdater external_policy_data_updater_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_COMPONENT_CLOUD_POLICY_UPDATER_H_
