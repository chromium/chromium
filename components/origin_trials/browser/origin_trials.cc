// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/origin_trials/browser/origin_trials.h"

#include <algorithm>

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "components/origin_trials/common/persisted_trial_token.h"
#include "net/base/schemeful_site.h"
#include "third_party/blink/public/common/origin_trials/origin_trial_feature.h"
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

bool OriginTrials::IsFeaturePersistedForOrigin(
    const url::Origin& origin,
    const url::Origin& partition_origin,
    blink::OriginTrialFeature feature,
    const base::Time current_time) {
  return !GetPersistedTrialsForOriginWithMatch(origin, partition_origin,
                                               current_time, feature)
              .empty();
}

void OriginTrials::PersistTrialsFromTokens(
    const url::Origin& origin,
    const url::Origin& partition_origin,
    const base::span<const std::string> header_tokens,
    const base::Time current_time) {
  PersistTokensInternal(origin, partition_origin, /*script_origins=*/{},
                        header_tokens, current_time,
                        /*append_only=*/false);
}

void OriginTrials::PersistAdditionalTrialsFromTokens(
    const url::Origin& origin,
    const url::Origin& partition_origin,
    base::span<const url::Origin> script_origins,
    const base::span<const std::string> header_tokens,
    const base::Time current_time) {
  PersistTokensInternal(origin, partition_origin, script_origins, header_tokens,
                        current_time,
                        /*append_only=*/true);
}

void OriginTrials::PersistTokensInternal(
    const url::Origin& origin,
    const url::Origin& partition_origin,
    base::span<const url::Origin> script_origins,
    const base::span<const std::string> header_tokens,
    const base::Time current_time,
    bool append_only) {
  if (origin.opaque()) {
    return;
  }

  base::flat_map<url::Origin, std::vector<blink::TrialToken>> valid_tokens;
  if (!append_only) {
    // Explicitly initialize the entry for the first-party origin since an empty
    // vector means tokens should be cleared if |append_only| is false.
    valid_tokens[origin] = {};
  }

  // Parse the provided tokens
  for (const base::StringPiece token : header_tokens) {
    blink::TrialTokenResult validation_result =
        trial_token_validator_->ValidateTokenAndTrial(
            token, origin, script_origins, current_time);

    const blink::TrialToken* parsed_token = validation_result.ParsedToken();

    if (validation_result.Status() != blink::OriginTrialTokenStatus::kSuccess) {
      continue;
    }
    if (!blink::origin_trials::IsTrialPersistentToNextResponse(
            parsed_token->feature_name())) {
      continue;
    }
    // TODO(crbug.com/1227440): Should be part of general validation logic.
    if (!trial_token_validator_->TrialEnablesFeaturesForOS(
            parsed_token->feature_name())) {
      continue;
    }
    if (parsed_token->is_third_party()) {
      // TODO(crbug.com/1418340): Support for all third-party tokens.
      // Only accept deprecation trials as third-party for now.
      bool deprecation_trial = false;
      for (const blink::OriginTrialFeature feature :
           blink::origin_trials::FeaturesForTrial(
               parsed_token->feature_name())) {
        deprecation_trial |= blink::origin_trials::GetTrialType(feature) ==
                             blink::OriginTrialType::kDeprecation;
      }
      if (deprecation_trial) {
        // Valid third-party tokens are saved using the origin stored in the
        // token.
        valid_tokens[parsed_token->origin()].push_back(
            std::move(*parsed_token));
      }
    } else {
      // First party tokens use the passed-in origin, since it could be a
      // subdomain.
      valid_tokens[origin].push_back(std::move(*parsed_token));
    }
  }
  std::string partition_site = GetTokenPartitionSite(partition_origin);
  for (const auto& origin_token_pair : valid_tokens) {
    UpdatePersistedTokenSet(origin_token_pair.first, origin_token_pair.second,
                            partition_site, append_only);
  }
}

base::flat_set<std::string> OriginTrials::GetPersistedTrialsForOriginWithMatch(
    const url::Origin& origin,
    const url::Origin& partition_origin,
    const base::Time current_time,
    const absl::optional<blink::OriginTrialFeature> trial_feature_match) const {
  if (origin.opaque())
    return {};

  base::flat_set<PersistedTrialToken> saved_tokens =
      persistence_provider_->GetPersistentTrialTokens(origin);

  base::flat_set<std::string> enabled_trials;
  for (const PersistedTrialToken& token : saved_tokens) {
    if (trial_feature_match &&
        // TODO(crbug.com/1227440): FeaturesEnabledByTrial should be part of
        // general validation logic.
        !base::Contains(
            trial_token_validator_->FeaturesEnabledByTrial(token.trial_name),
            trial_feature_match.value())) {
      continue;
    }

    bool valid = trial_token_validator_->RevalidateTokenAndTrial(
        token.trial_name, token.token_expiry, token.usage_restriction,
        token.token_signature, current_time);
    bool persistent =
        blink::origin_trials::IsTrialPersistentToNextResponse(token.trial_name);
    if (valid && persistent &&
        token.partition_sites.contains(
            GetTokenPartitionSite(partition_origin))) {
      // Move the string into the flat_set to avoid extra heap allocations
      enabled_trials.insert(std::move(token.trial_name));
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

void OriginTrials::UpdatePersistedTokenSet(
    const url::Origin& origin,
    base::span<const blink::TrialToken> new_tokens,
    const std::string& partition_site,
    bool append_only) {
  if (append_only && new_tokens.empty()) {
    return;  // Nothing to do.
  }

  base::flat_set<PersistedTrialToken> token_set =
      persistence_provider_->GetPersistentTrialTokens(origin);

  if (!append_only) {
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
  }

  // Update the set with new partition information.
  for (const blink::TrialToken& new_token : new_tokens) {
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
  persistence_provider_->SavePersistentTrialTokens(origin,
                                                   std::move(token_set));
}

}  // namespace origin_trials
