// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/component_cloud_policy_updater.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "components/policy/core/common/cloud/component_cloud_policy_store.h"
#include "components/policy/core/common/cloud/external_policy_data_fetcher.h"
#include "components/policy/core/common/policy_logger.h"
#include "components/policy/proto/chrome_extension_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace em = enterprise_management;

namespace policy {

namespace {

// The maximum size of the serialized policy protobuf.
const size_t kPolicyProtoMaxSize = 16 * 1024;

// The maximum size of the downloaded policy data.
const int64_t kPolicyDataMaxSize = 5 * 1024 * 1024;

// Tha maximum number of policy data fetches to run in parallel.
const int64_t kMaxParallelPolicyDataFetches = 2;

std::string NamespaceToKey(const PolicyNamespace& ns) {
  const std::string domain = base::NumberToString(ns.domain);
  const std::string size = base::NumberToString(domain.size());
  return size + ":" + domain + ":" + ns.component_id;
}

}  // namespace

ComponentCloudPolicyUpdater::ComponentCloudPolicyUpdater(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    std::unique_ptr<ExternalPolicyDataFetcher> external_policy_data_fetcher,
    ComponentCloudPolicyStore* store)
    : store_(store),
      external_policy_data_updater_(task_runner,
                                    std::move(external_policy_data_fetcher),
                                    kMaxParallelPolicyDataFetches) {}

ComponentCloudPolicyUpdater::~ComponentCloudPolicyUpdater() {
}

void ComponentCloudPolicyUpdater::UpdateExternalPolicy(
    const PolicyNamespace& ns,
    std::unique_ptr<em::PolicyFetchResponse> response) {
  // Keep a serialized copy of |response|, to cache it later.
  // The policy is also rejected if it exceeds the maximum size.
  std::string serialized_response;
  if (!response->SerializeToString(&serialized_response)) {
    LOG_POLICY(ERROR, CBCM_ENROLLMENT)
        << "Failed to serialize policy fetch response.";
    return;
  }
  if (serialized_response.size() > kPolicyProtoMaxSize) {
    LOG_POLICY(ERROR, CBCM_ENROLLMENT)
        << "Policy fetch response too large: " << serialized_response.size()
        << " bytes (max " << kPolicyProtoMaxSize << ").";
    return;
  }

  // Validate the policy before doing anything else.
  auto policy_data = std::make_unique<em::PolicyData>();
  em::ExternalPolicyData data;
  std::string error;
  if (!store_->ValidatePolicy(ns, std::move(response), policy_data.get(), &data,
                              &error)) {
    LOG_POLICY(ERROR, CBCM_ENROLLMENT)
        << "Discarding policy for component " << ns.component_id
        << " due to policy validation failure: " << error;
    return;
  }

  // Maybe the data for this hash has already been downloaded and cached.
  const std::string& cached_hash = store_->GetCachedHash(ns);
  if (!cached_hash.empty() && data.secure_hash() == cached_hash)
    return;

  const std::string key = NamespaceToKey(ns);

  if (data.download_url().empty() || !data.has_secure_hash()) {
    // If there is no policy for this component or the policy has been removed,
    // cancel any existing request to fetch policy for this component.
    LOG_POLICY(WARNING, CBCM_ENROLLMENT)
        << "Cancelling existing request to fetch policy for component: "
        << ns.component_id
        << "there is no pilicy for component or the policy has been removed.";
    external_policy_data_updater_.CancelExternalDataFetch(key);

    // Delete any existing policy for this component.
    store_->Delete(ns);
  } else {
    // Make a request to fetch policy for this component. If another fetch
    // request is already pending for the component, it will be canceled.
    LOG_POLICY(INFO, CBCM_ENROLLMENT)
        << "Fetching policy for component: " << ns.component_id;
    external_policy_data_updater_.FetchExternalData(
        key,
        ExternalPolicyDataUpdater::Request(
            data.download_url(), data.secure_hash(), kPolicyDataMaxSize),
        base::BindRepeating(&ComponentCloudPolicyStore::Store,
                            base::Unretained(store_), ns, serialized_response,
                            base::Owned(policy_data.release()),
                            data.secure_hash()));
  }
}

void ComponentCloudPolicyUpdater::CancelUpdate(const PolicyNamespace& ns) {
  external_policy_data_updater_.CancelExternalDataFetch(NamespaceToKey(ns));
}

}  // namespace policy
