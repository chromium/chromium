// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/origin_trials/browser/origin_trials.h"

#include <algorithm>

#include "base/containers/flat_set.h"
#include "components/origin_trials/common/persisted_trial_token.h"
#include "net/base/schemeful_site.h"
#include "third_party/blink/public/common/origin_trials/origin_trials.h"
#include "third_party/blink/public/common/origin_trials/trial_token.h"
#include "third_party/blink/public/common/origin_trials/trial_token_result.h"
#include "third_party/blink/public/common/origin_trials/trial_token_validator.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace origin_trials {

namespace {
// A string to denote opaque origins for partitioning. It should not be
// possible to have a valid origin serialize to this value.
const char kOpaqueOriginPartitionKey[] = ":opaque";
}  // namespace

OriginTrials::OriginTrials(
    std::unique_ptr<OriginTrialsPersistenceProvider> persistence_provider,
    std::unique_ptr<blink::TrialTokenValidator> token_validator)
    : persistence_provider_(std::move(persistence_provider)),
      trial_token_validator_(std::move(token_validator)) {}

OriginTrials::~OriginTrials() = default;

base::flat_set<std::string> OriginTrials::GetPersistedTrialsForOrigin(
    const url::Origin& origin,
    const url::Origin& partition_origin,
    const base::Time current_time) {
  return GetPersistedTrialsForOriginWithMatch(origin, partition_origin,
                                              current_time, absl::nullopt);
}

bool OriginTrials::IsTrialPersistedForOrigin(
    const url::Origin& origin,
    const url::Origin& partition_origin,
    const base::StringPiece trial_name,
    const base::Time current_time) {
  return !GetPersistedTrialsForOriginWithMatch(origin, partition_origin,
                                               current_time, trial_name)
              .empty();
}

void OriginTrials::PersistTrialsFromTokens(
    const url::Origin& origin,
    const url::Origin& partition_origin,
    const base::span<const std::string> header_tokens,
    const base::Time current_time) {
  if (origin.opaque()) {
    return;
  }

  base::flat_set<PersistedTrialToken> existing_tokens =
      persistence_provider_->GetPersistentTrialTokens(origin);

  std::vector<blink::TrialToken> valid_tokens;

  for (const base::StringPiece token : header_tokens) {
    blink::TrialTokenResult validation_result =
        trial_token_validator_->ValidateTokenAndTrial(token, origin,
                                                      current_time);

    const blink::TrialToken* parsed_token = validation_result.ParsedToken();
    if (validation_result.Status() == blink::OriginTrialTokenStatus::kSuccess &&
        blink::origin_trials::IsTrialPersistentToNextResponse(
            parsed_token->feature_name())) {
      valid_tokens.push_back(std::move(*parsed_token));
    }
  }
  UpdatePersistedTokenSet(existing_tokens, std::move(valid_tokens),
                          GetTokenPartitionSite(partition_origin));
  persistence_provider_->SavePersistentTrialTokens(origin,
                                                   std::move(existing_tokens));
}

base::flat_set<std::string> OriginTrials::GetPersistedTrialsForOriginWithMatch(
    const url::Origin& origin,
    const url::Origin& partition_origin,
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
      if (valid && persistent &&
          token.partition_sites.contains(
              GetTokenPartitionSite(partition_origin))) {
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

// static
std::string OriginTrials::GetTokenPartitionSite(const url::Origin& origin) {
  if (origin.opaque()) {
    return kOpaqueOriginPartitionKey;
  }
  return net::SchemefulSite(origin).Serialize();
}

// static
void OriginTrials::UpdatePersistedTokenSet(
    base::flat_set<PersistedTrialToken>& token_set,
    std::vector<blink::TrialToken> new_tokens,
    std::string partition_site) {
  // First, clean up token registrations for this origin and partition
  // by removing any trials in the active partition that aren't being set
  // by the new parameters.
  for (PersistedTrialToken& token : token_set) {
    const auto new_token_iter =
        std::find_if(new_tokens.begin(), new_tokens.end(),
                     [&token](const blink::TrialToken& trial_token) {
                       return token.Matches(trial_token);
                     });

    // Remove registration of the token for the first party or top-level site
    // partition.
    if (new_token_iter == new_tokens.end()) {
      token.RemoveFromPartition(partition_site);
    }
  }
  // Cleanup of tokens no longer in any partitions.
  base::EraseIf(token_set, [](const PersistedTrialToken& token) {
    return !token.InAnyPartition();
  });

  // Update the set with new partition information.
  for (blink::TrialToken& new_token : new_tokens) {
    const auto found_token =
        std::find_if(token_set.begin(), token_set.end(),
                     [&new_token](const PersistedTrialToken& existing_token) {
                       return existing_token.Matches(new_token);
                     });

    if (found_token != token_set.end()) {
      // Update the existing stored trial token with the metadata fields, as it
      // may be a newly issued token.
      found_token->AddToPartition(partition_site);
    } else {
      token_set.emplace(new_token, partition_site);
    }
  }
}

}  // namespace origin_trials
