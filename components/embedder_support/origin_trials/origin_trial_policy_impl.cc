// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/embedder_support/origin_trials/origin_trial_policy_impl.h"

#include <stdint.h>
#include <vector>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/cxx17_backports.h"
#include "base/feature_list.h"
#include "base/memory/raw_ref.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "components/embedder_support/origin_trials/features.h"
#include "components/embedder_support/switches.h"
#include "content/public/common/content_features.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"

namespace embedder_support {

// This is the default public key used for validating signatures.
static const blink::OriginTrialPublicKey kDefaultPublicKey = {
    0x7c, 0xc4, 0xb8, 0x9a, 0x93, 0xba, 0x6e, 0xe2, 0xd0, 0xfd, 0x03,
    0x1d, 0xfb, 0x32, 0x66, 0xc7, 0x3b, 0x72, 0xfd, 0x54, 0x3a, 0x07,
    0x51, 0x14, 0x66, 0xaa, 0x02, 0x53, 0x4e, 0x33, 0xa1, 0x15,
};

OriginTrialPolicyImpl::OriginTrialPolicyImpl() {
  public_keys_.push_back(kDefaultPublicKey);

  // Set the public key and disabled feature list for the origin trial key
  // manager, based on the command line flags which were passed to this process.
  // If the flags are not present, or are incorrectly formatted, the defaults
  // will remain active.
  if (base::CommandLine::InitializedForCurrentProcess()) {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(kOriginTrialPublicKey)) {
      SetPublicKeysFromASCIIString(
          command_line->GetSwitchValueASCII(kOriginTrialPublicKey));
    }
    if (command_line->HasSwitch(kOriginTrialDisabledFeatures)) {
      SetDisabledFeatures(
          command_line->GetSwitchValueASCII(kOriginTrialDisabledFeatures));
    }
    if (command_line->HasSwitch(kOriginTrialDisabledTokens)) {
      SetDisabledTokens(
          command_line->GetSwitchValueASCII(kOriginTrialDisabledTokens));
    }
  }
}

OriginTrialPolicyImpl::~OriginTrialPolicyImpl() = default;

bool OriginTrialPolicyImpl::IsOriginTrialsSupported() const {
  return true;
}

const std::vector<blink::OriginTrialPublicKey>&
OriginTrialPolicyImpl::GetPublicKeys() const {
  return public_keys_;
}

bool OriginTrialPolicyImpl::IsFeatureDisabled(base::StringPiece feature) const {
  return disabled_features_.count(std::string(feature)) > 0;
}

bool OriginTrialPolicyImpl::IsTokenDisabled(
    base::StringPiece token_signature) const {
  return disabled_tokens_.count(std::string(token_signature)) > 0;
}

// Exclude users in Field trial control group from the corresponding origin
// trial. Users from experiment group/default group will have access to the
// origin trial.
bool OriginTrialPolicyImpl::IsFeatureDisabledForUser(
    base::StringPiece feature) const {
  struct OriginTrialFeatureToBaseFeatureMap {
    const char* origin_trial_feature_name;
    const base::Feature& field_trial_feature;
  } origin_trial_feature_to_field_trial_feature_map[] = {
      {"FrobulateThirdParty", kOriginTrialsSampleAPIThirdPartyAlternativeUsage},
      {"ConversionMeasurement", kConversionMeasurementAPIAlternativeUsage}};
  for (const auto& mapping : origin_trial_feature_to_field_trial_feature_map) {
    if (feature == mapping.origin_trial_feature_name) {
      return !base::FeatureList::IsEnabled(mapping.field_trial_feature);
    }
  }
  return false;
}

bool OriginTrialPolicyImpl::IsOriginSecure(const GURL& url) const {
  return network::IsUrlPotentiallyTrustworthy(url);
}

bool OriginTrialPolicyImpl::SetPublicKeysFromASCIIString(
    const std::string& ascii_public_keys) {
  std::vector<blink::OriginTrialPublicKey> new_public_keys;
  const auto public_keys = base::SplitString(
      ascii_public_keys, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (const auto& ascii_public_key : public_keys) {
    // Base64-decode the incoming string. Set the key if it is correctly
    // formatted
    std::string new_public_key;
    blink::OriginTrialPublicKey binary_new_public_key;

    if (!base::Base64Decode(ascii_public_key, &new_public_key))
      return false;
    if (new_public_key.size() != binary_new_public_key.size())
      return false;

    std::copy_n(new_public_key.data(), binary_new_public_key.size(),
                binary_new_public_key.begin());

    new_public_keys.push_back(binary_new_public_key);
  }
  if (new_public_keys.size() > 0) {
    public_keys_.swap(new_public_keys);
    return true;
  }
  return false;
}

bool OriginTrialPolicyImpl::SetDisabledFeatures(
    const std::string& disabled_feature_list) {
  std::set<std::string> new_disabled_features;
  const std::vector<std::string> features =
      base::SplitString(disabled_feature_list, "|", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);
  for (const std::string& feature : features)
    new_disabled_features.insert(feature);
  disabled_features_.swap(new_disabled_features);
  return true;
}

bool OriginTrialPolicyImpl::SetDisabledTokens(
    const std::string& disabled_token_list) {
  std::set<std::string> new_disabled_tokens;
  const std::vector<std::string> tokens =
      base::SplitString(disabled_token_list, "|", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);
  for (const std::string& ascii_token : tokens) {
    std::string token_signature;
    if (!base::Base64Decode(ascii_token, &token_signature))
      continue;
    if (token_signature.size() != 64)
      continue;
    new_disabled_tokens.insert(token_signature);
  }
  disabled_tokens_.swap(new_disabled_tokens);
  return true;
}

}  // namespace embedder_support
