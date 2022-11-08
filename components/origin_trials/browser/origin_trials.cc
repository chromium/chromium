// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/origin_trials/browser/origin_trials.h"

#include <algorithm>

#include "components/origin_trials/common/persisted_trial_token.h"
#include "third_party/blink/public/common/origin_trials/origin_trials.h"
#include "third_party/blink/public/common/origin_trials/trial_token.h"
#include "third_party/blink/public/common/origin_trials/trial_token_result.h"
#include "third_party/blink/public/common/origin_trials/trial_token_validator.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace origin_trials {

OriginTrials::OriginTrials(
    std::unique_ptr<OriginTrialsPersistenceProvider> persistence_provider,
    std::unique_ptr<blink::TrialTokenValidator> token_validator)
    : persistence_provider_(std::move(persistence_provider)),
      trial_token_validator_(std::move(token_validator)) {}

OriginTrials::~OriginTrials() = default;

base::flat_set<std::string> OriginTrials::GetPersistedTrialsForOrigin(
    const url::Origin& origin,
    const base::Time current_time) {
  return GetPersistedTrialsForOriginWithMatch(origin, current_time,
                                              absl::nullopt);
}

bool OriginTrials::IsTrialPersistedForOrigin(const url::Origin& origin,
                                             const base::StringPiece trial_name,
                                             const base::Time current_time) {
  return !GetPersistedTrialsForOriginWithMatch(origin, current_time, trial_name)
              .empty();
}

void OriginTrials::PersistTrialsFromTokens(
    const url::Origin& origin,
    const base::span<const std::string> header_tokens,
    const base::Time current_time) {
  if (origin.opaque())
    return;

  base::flat_set<PersistedTrialToken> enabled_persistent_trial_tokens;

  for (const base::StringPiece token : header_tokens) {
    blink::TrialTokenResult validation_result =
        trial_token_validator_->ValidateTokenAndTrial(token, origin,
                                                      current_time);
    const blink::TrialToken* parsed_token = validation_result.ParsedToken();
    if (validation_result.Status() == blink::OriginTrialTokenStatus::kSuccess &&
        blink::origin_trials::IsTrialPersistentToNextResponse(
            parsed_token->feature_name())) {
      enabled_persistent_trial_tokens.emplace(
          parsed_token->feature_name(), parsed_token->expiry_time(),
          parsed_token->usage_restriction(), parsed_token->signature());
    }
  }
  persistence_provider_->SavePersistentTrialTokens(
      origin, std::move(enabled_persistent_trial_tokens));
}

base::flat_set<std::string> OriginTrials::GetPersistedTrialsForOriginWithMatch(
    const url::Origin& origin,
    const base::Time current_time,
    const absl::optional<const base::StringPiece> trial_name_match) const {
  if (origin.opaque())
    return {};

  base::flat_set<PersistedTrialToken> saved_tokens =
      persistence_provider_->GetPersistentTrialTokens(origin);

  base::flat_set<std::string> enabled_trials;
  for (const PersistedTrialToken& token : saved_tokens) {
    if (!trial_name_match || token.trial_name == *trial_name_match) {
      bool valid = trial_token_validator_->RevalidateTokenAndTrial(
          token.trial_name, token.token_expiry, token.usage_restriction,
          token.token_signature, current_time);
      bool persistent = blink::origin_trials::IsTrialPersistentToNextResponse(
          token.trial_name);
      if (valid && persistent) {
        // Move the string into the flat_set to avoid extra heap allocations
        enabled_trials.insert(std::move(token.trial_name));
      }
    }
  }

  return enabled_trials;
}

void OriginTrials::ClearPersistedTokens() {
  persistence_provider_->ClearPersistedTokens();
}

}  // namespace origin_trials
