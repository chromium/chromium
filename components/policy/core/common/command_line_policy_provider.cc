// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/command_line_policy_provider.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_types.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace policy {

// static
std::unique_ptr<CommandLinePolicyProvider>
CommandLinePolicyProvider::CreateIfAllowed(
    const base::CommandLine& command_line,
    version_info::Channel channel) {
#if BUILDFLAG(IS_ANDROID)
  if (channel == version_info::Channel::STABLE ||
      channel == version_info::Channel::BETA) {
    return nullptr;
  }

  if (!base::android::BuildInfo::GetInstance()->is_debug_android())
    return nullptr;

  return base::WrapUnique(new CommandLinePolicyProvider(command_line));
#else
  return nullptr;
#endif  // BUILDFLAG(IS_ANDROID)
}

// static
std::unique_ptr<CommandLinePolicyProvider>
CommandLinePolicyProvider::CreateForTesting(
    const base::CommandLine& command_line) {
  return base::WrapUnique(new CommandLinePolicyProvider(command_line));
}

CommandLinePolicyProvider::~CommandLinePolicyProvider() = default;

void CommandLinePolicyProvider::RefreshPolicies(PolicyFetchReason reason) {
  PolicyBundle bundle = loader_.Load();
  first_policies_loaded_ = true;
  UpdatePolicy(std::move(bundle));
}

bool CommandLinePolicyProvider::IsFirstPolicyLoadComplete(
    PolicyDomain domain) const {
  return first_policies_loaded_;
}

CommandLinePolicyProvider::CommandLinePolicyProvider(
    const base::CommandLine& command_line)
    : loader_(command_line) {
  RefreshPolicies(PolicyFetchReason::kBrowserStart);
}

}  // namespace policy
