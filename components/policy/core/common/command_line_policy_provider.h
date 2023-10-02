// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_COMMAND_LINE_POLICY_PROVIDER_H_
#define COMPONENTS_POLICY_CORE_COMMON_COMMAND_LINE_POLICY_PROVIDER_H_

#include "components/policy/core/common/configuration_policy_provider.h"
#include "components/policy/core/common/policy_loader_command_line.h"
#include "components/policy/policy_export.h"
#include "components/version_info/channel.h"

namespace policy {

// The policy provider for the Command Line Policy which is used for development
// and testing purposes.
class POLICY_EXPORT CommandLinePolicyProvider
    : public ConfigurationPolicyProvider {
 public:
  // The |CommandLinePolicyProvider| provides an extremely easy way to set up
  // policies which means it can be used for malicious purposes. So it should
  // be created if and only if the browser is under development environment.
  static std::unique_ptr<CommandLinePolicyProvider> CreateIfAllowed(
      const base::CommandLine& command_line,
      version_info::Channel channel);

  static std::unique_ptr<CommandLinePolicyProvider> CreateForTesting(
      const base::CommandLine& command_line);

  CommandLinePolicyProvider(const CommandLinePolicyProvider&) = delete;
  CommandLinePolicyProvider& operator=(const CommandLinePolicyProvider&) =
      delete;

  ~CommandLinePolicyProvider() override;

  // ConfigurationPolicyProvider implementation.
  void RefreshPolicies(PolicyFetchReason reason) override;
  bool IsFirstPolicyLoadComplete(PolicyDomain domain) const override;

 private:
  explicit CommandLinePolicyProvider(const base::CommandLine& command_line);

  bool first_policies_loaded_;
  PolicyLoaderCommandLine loader_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_COMMAND_LINE_POLICY_PROVIDER_H_
