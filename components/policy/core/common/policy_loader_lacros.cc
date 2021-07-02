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
#include "chromeos/lacros/lacros_chrome_service_impl.h"
#include "components/policy/core/common/cloud/cloud_policy_validator.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_proto_decoders.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace policy {

PolicyLoaderLacros::PolicyLoaderLacros(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : AsyncPolicyLoader(task_runner, /*periodic_updates=*/false),
      task_runner_(task_runner) {
  auto* lacros_chrome_service = chromeos::LacrosChromeServiceImpl::Get();
  const crosapi::mojom::BrowserInitParams* init_params =
      lacros_chrome_service->init_params();
  if (!init_params) {
    LOG(ERROR) << "No init params";
    return;
  }
  if (!init_params->device_account_policy) {
    LOG(ERROR) << "No policy data";
    return;
  }
  policy_fetch_response_ = init_params->device_account_policy.value();
}

PolicyLoaderLacros::~PolicyLoaderLacros() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto* lacros_chrome_service = chromeos::LacrosChromeServiceImpl::Get();
  if (lacros_chrome_service) {
    lacros_chrome_service->RemoveObserver(this);
  }
}

void PolicyLoaderLacros::InitOnBackgroundThread() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DETACH_FROM_SEQUENCE(sequence_checker_);
  // We add this as observer on background thread to avoid a situation when
  // notification comes after the object is destroyed, but not removed from the
  // list yet.
  // TODO(crbug.com/1114069): Set up LacrosChromeServiceImpl in tests.
  auto* lacros_chrome_service = chromeos::LacrosChromeServiceImpl::Get();
  if (lacros_chrome_service) {
    lacros_chrome_service->AddObserver(this);
  }
}

std::unique_ptr<PolicyBundle> PolicyLoaderLacros::Load() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::unique_ptr<PolicyBundle> bundle = std::make_unique<PolicyBundle>();

  if (!policy_fetch_response_ || policy_fetch_response_->empty()) {
    return bundle;
  }

  auto policy = std::make_unique<enterprise_management::PolicyFetchResponse>();
  if (!policy->ParseFromArray(policy_fetch_response_.value().data(),
                              policy_fetch_response_->size())) {
    LOG(ERROR) << "Failed to parse policy data";
    return bundle;
  }
  UserCloudPolicyValidator validator(std::move(policy),
                                     /*background_task_runner=*/nullptr);
  validator.ValidatePayload();
  validator.RunValidation();

  PolicyMap policy_map;
  base::WeakPtr<CloudExternalDataManager> external_data_manager;
  DecodeProtoFields(*(validator.payload()), external_data_manager,
                    PolicySource::POLICY_SOURCE_CLOUD_FROM_ASH,
                    PolicyScope::POLICY_SCOPE_USER, &policy_map,
                    PolicyPerProfileFilter::kFalse);
  SetEnterpriseUsersSystemWideDefaults(&policy_map);
  bundle->Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
      .MergeFrom(policy_map);
  return bundle;
}

void PolicyLoaderLacros::OnPolicyUpdated(
    const std::vector<uint8_t>& policy_fetch_response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  policy_fetch_response_ = policy_fetch_response;
  Reload(true);
}

}  // namespace policy
