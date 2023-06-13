// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/local_test_policy_provider.h"

#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service_impl.h"

namespace policy {

// static
std::unique_ptr<LocalTestPolicyProvider>
LocalTestPolicyProvider::CreateIfAllowed(version_info::Channel channel) {
  if (channel == version_info::Channel::CANARY ||
      channel == version_info::Channel::DEFAULT) {
    return base::WrapUnique(new LocalTestPolicyProvider());
  }

#if BUILDFLAG(IS_IOS)
  if (channel == version_info::Channel::BETA) {
    return base::WrapUnique(new LocalTestPolicyProvider());
  }
#endif

  return nullptr;
}

LocalTestPolicyProvider::~LocalTestPolicyProvider() = default;

void LocalTestPolicyProvider::RefreshPolicies() {
  PolicyBundle bundle = loader_.Load();
  first_policies_loaded_ = true;
  UpdatePolicy(std::move(bundle));
}

bool LocalTestPolicyProvider::IsFirstPolicyLoadComplete(
    PolicyDomain domain) const {
  return first_policies_loaded_;
}

LocalTestPolicyProvider::LocalTestPolicyProvider() {
  RefreshPolicies();
}

}  // namespace policy
