// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_loader_lacros.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chromeos/lacros/lacros_chrome_service_impl.h"
#include "components/policy/core/common/cloud/cloud_policy_validator.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_proto_decoders.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace policy {

PolicyLoaderLacros::PolicyLoaderLacros(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : AsyncPolicyLoader(task_runner), task_runner_(task_runner) {}

PolicyLoaderLacros::~PolicyLoaderLacros() = default;

void PolicyLoaderLacros::InitOnBackgroundThread() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
}

std::unique_ptr<PolicyBundle> PolicyLoaderLacros::Load() {
  std::unique_ptr<PolicyBundle> bundle = std::make_unique<PolicyBundle>();

  auto* lacros_chrome_service = chromeos::LacrosChromeServiceImpl::Get();
  if (!lacros_chrome_service) {
    // LacrosChromeService should be available at this timing in production.
    // However, in some existing tests, it is not.
    // TODO(crbug.com/1114069): Set up LacrosChromeServiceImpl in tests.
    LOG(ERROR) << "No LacrosChromeService is found.";
    return bundle;
  }
  const crosapi::mojom::LacrosInitParams* init_params =
      lacros_chrome_service->init_params();
  if (!init_params) {
    LOG(ERROR) << "No init params";
    return bundle;
  }

  if (!init_params->device_account_policy) {
    LOG(ERROR) << "No policy data";
    return bundle;
  }

  std::vector<uint8_t> data = init_params->device_account_policy.value();
  if (data.empty()) {
    return bundle;
  }

  auto policy = std::make_unique<enterprise_management::PolicyFetchResponse>();
  if (!policy->ParseFromString(std::string(data.begin(), data.end()))) {
    LOG(ERROR) << "Failed to parse policy data";
    return bundle;
  }
  UserCloudPolicyValidator validator(std::move(policy), task_runner_);
  validator.ValidatePayload();
  validator.RunValidation();

  PolicyMap policy_map;
  base::WeakPtr<CloudExternalDataManager> external_data_manager;
  DecodeProtoFields(*(validator.payload()), external_data_manager,
                    PolicySource::POLICY_SOURCE_CLOUD,
                    PolicyScope::POLICY_SCOPE_USER, &policy_map);
  bundle->Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
      .MergeFrom(policy_map);
  last_modification_ = base::Time::Now();
  return bundle;
}

base::Time PolicyLoaderLacros::LastModificationTime() {
  return last_modification_;
}

}  // namespace policy
