// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EMBEDDER_SUPPORT_ORIGIN_TRIALS_ORIGIN_TRIAL_POLICY_IMPL_H_
#define COMPONENTS_EMBEDDER_SUPPORT_ORIGIN_TRIALS_ORIGIN_TRIAL_POLICY_IMPL_H_

#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "third_party/blink/public/common/origin_trials/origin_trial_policy.h"

namespace embedder_support {

// This class is instantiated on the main/ui thread, but its methods can be
// accessed from any thread.
class OriginTrialPolicyImpl : public blink::OriginTrialPolicy {
 public:
  explicit OriginTrialPolicyImpl();

  OriginTrialPolicyImpl(const OriginTrialPolicyImpl&) = delete;
  OriginTrialPolicyImpl& operator=(const OriginTrialPolicyImpl&) = delete;

  ~OriginTrialPolicyImpl() override;

  // blink::OriginTrialPolicy interface
  bool IsOriginTrialsSupported() const override;
  const std::vector<blink::OriginTrialPublicKey>& GetPublicKeys()
      const override;
  bool IsFeatureDisabled(std::string_view feature) const override;
  bool IsFeatureDisabledForUser(std::string_view feature) const override;
  bool IsTokenDisabled(std::string_view token_signature) const override;
  bool IsOriginSecure(const GURL& url) const override;

  bool SetPublicKeysFromASCIIString(const std::string& ascii_public_key);
  bool SetDisabledFeatures(const std::string& disabled_feature_list);
  bool SetDisabledTokens(const std::vector<std::string>& tokens);
  // Disabling deprecation trial could cause potential breakage. This
  // function allow embedder to safely disable all trials with
  // new/experimental features. By default all trials are allowed to run.
  void SetAllowOnlyDeprecationTrials(bool allow_only_deprecation_trials);
  bool GetAllowOnlyDeprecationTrials() const;
  const std::set<std::string>* GetDisabledTokensForTesting() const override;

 private:
  std::vector<blink::OriginTrialPublicKey> public_keys_;
  std::set<std::string> disabled_features_;
  std::set<std::string> disabled_tokens_;
  bool allow_only_deprecation_trials_ = false;
};

}  // namespace embedder_support

#endif  // COMPONENTS_EMBEDDER_SUPPORT_ORIGIN_TRIALS_ORIGIN_TRIAL_POLICY_IMPL_H_
