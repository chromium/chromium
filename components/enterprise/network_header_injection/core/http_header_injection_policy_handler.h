// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_NETWORK_HEADER_INJECTION_CORE_HTTP_HEADER_INJECTION_POLICY_HANDLER_H_
#define COMPONENTS_ENTERPRISE_NETWORK_HEADER_INJECTION_CORE_HTTP_HEADER_INJECTION_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/core/common/schema.h"

namespace enterprise_custom_headers {

// Policy handler for the HttpHeaderInjection policy.
// Validates that URL patterns are correct and header names are not forbidden.
class HttpHeaderInjectionPolicyHandler
    : public policy::SimpleSchemaValidatingPolicyHandler {
 public:
  explicit HttpHeaderInjectionPolicyHandler(policy::Schema schema);
  HttpHeaderInjectionPolicyHandler(const HttpHeaderInjectionPolicyHandler&) =
      delete;
  HttpHeaderInjectionPolicyHandler& operator=(
      const HttpHeaderInjectionPolicyHandler&) = delete;
  ~HttpHeaderInjectionPolicyHandler() override;

  // ConfigurationPolicyHandler:
  bool CheckPolicySettings(const policy::PolicyMap& policies,
                           policy::PolicyErrorMap* errors) override;

  void ApplyPolicySettings(const policy::PolicyMap& policies,
                           PrefValueMap* prefs) override;
};

}  // namespace enterprise_custom_headers

#endif  // COMPONENTS_ENTERPRISE_NETWORK_HEADER_INJECTION_CORE_HTTP_HEADER_INJECTION_POLICY_HANDLER_H_
