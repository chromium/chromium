// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_COMMON_SAFE_BROWSING_POLICY_HANDLER_H_
#define COMPONENTS_SAFE_BROWSING_CORE_COMMON_SAFE_BROWSING_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

class PrefService;

namespace safe_browsing {

// Handles the SafeBrowsingEnabled and SafeBrowsingProtectionLevel policies.
// Controls the managed values of the prefs |kSafeBrowsingEnabled| and
// |kSafeBrowsingEnhanced|.
class SafeBrowsingPolicyHandler : public policy::ConfigurationPolicyHandler {
 public:
  SafeBrowsingPolicyHandler() = default;
  ~SafeBrowsingPolicyHandler() override = default;
  SafeBrowsingPolicyHandler(const SafeBrowsingPolicyHandler&) = delete;
  SafeBrowsingPolicyHandler& operator=(const SafeBrowsingPolicyHandler&) =
      delete;

  // Safe Browsing protection level as set by policy. The values must match the
  // 'SafeBrowsingProtectionLevel' policy definition.
  enum class ProtectionLevel {
    // Safe Browsing is disabled.
    // Prior to M83, this was set by setting the SafeBrowsingEnabled policy to
    // |false| or 0x00.
    kNoProtection = 0,

    // Default: Standard Safe Browsing is enabled.
    // Prior to M83, this was set by setting the SafeBrowsingEnabled policy to
    // |true| or 0x01.
    kStandardProtection = 1,

    // Enhanced Protection is enabled. This provides the strongest Safe Browsing
    // protection but requires sending more data to Google.
    kEnhancedProtection = 2,

    // Maximal valid value for range checking.
    kMaxValue = kEnhancedProtection
  };

  // ConfigurationPolicyHandler methods:
  bool CheckPolicySettings(const policy::PolicyMap& policies,
                           policy::PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const policy::PolicyMap& policies,
                           PrefValueMap* prefs) override;

  // Returns the current policy-set Safe Browsing protection level according to
  // the values in |pref_service|. If no policy mandating Safe Browsing
  // protection level is set, the default will be
  // |ProtectionLevel::kStandardProtection|.
  static ProtectionLevel GetSafeBrowsingProtectionLevel(
      const PrefService* pref_service);

  // Returns true if Safe Browsing protection level is set by an active policy
  // in |pref_service|.
  static bool IsSafeBrowsingProtectionLevelSetByPolicy(
      const PrefService* pref_service);
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_COMMON_SAFE_BROWSING_POLICY_HANDLER_H_
