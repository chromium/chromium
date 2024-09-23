// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/origin_trials/browser/origin_trials.h"

#include <algorithm>
#include <string_view>

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "components/origin_trials/common/persisted_trial_token.h"
#include "net/base/schemeful_site.h"
#include "third_party/blink/public/common/origin_trials/origin_trials.h"
#include "third_party/blink/public/common/origin_trials/trial_token.h"
#include "third_party/blink/public/common/origin_trials/trial_token_result.h"
#include "third_party/blink/public/common/origin_trials/trial_token_validator.h"
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

void OriginTrials::AddObserver(Observer* observer) {
  CHECK(observer);
  observer_map_[observer->trial_name()].AddObserver(observer);
}

void OriginTrials::RemoveObserver(Observer* observer) {
  CHECK(observer);
  auto it = observer_map_.find(observer->trial_name());
  if (it == observer_map_.end()) {
    return;
  }
  it->second.RemoveObserver(observer);
  if (it->second.empty()) {
    observer_map_.erase(it);
  }
}

void OriginTrials::NotifyStatusChange(
    const std::string& trial,
    const OriginTrialStatusChangeDetails& details) {
  const auto find_it = observer_map_.find(trial);
  if (find_it == observer_map_.end()) {
    return;
  }

  for (Observer& observer : find_it->second) {
    observer.OnStatusChanged(details);
  }
}

void OriginTrials::NotifyPersistedTokensCleared() {
  for (const auto& it : observer_map_) {
    for (Observer& observer : it.second) {
      observer.OnPersistedTokensCleared();
    }
  }
}

bool OriginTrials::MatchesTokenOrigin(const url::Origin& token_origin,
                                      bool match_subdomains,
                                      const url::Origin& origin) const {
  if (match_subdomains) {
    return origin.scheme() == token_origin.scheme() &&
           origin.DomainIs(token_origin.host()) &&
           origin.port() == token_origin.port();
  }

  return origin == token_origin;
}

base::flat_set<std::string> OriginTrials::GetPersistedTrialsForOrigin(
    const url::Origin& origin,
    const url::Origin& partition_origin,
    const base::Time current_time) {
  return GetPersistedTrialsForOriginWithMatch(origin, partition_origin,
                                              current_time, std::nullopt);
}

bool OriginTrials::IsFeaturePersistedForOrigin(
    const url::Origin& origin,
    const url::Origin& partition_origin,
    blink::mojom::OriginTrialFeature feature,
    const base::Time current_time) {
  return !GetPersistedTrialsForOriginWithMatch(origin, partition_origin,
                                               current_time, feature)
              .empty();
}

void OriginTrials::PersistTrialsFromTokens(
    const url::Origin& origin,
    const url::Origin& partition_origin,
    const base::span<const std::string> header_tokens,
    const base::Time current_time,
    std::optional<ukm::SourceId> source_id) {
  PersistTokensInternal(origin, partition_origin, /*script_origins=*/{},
                        header_tokens, current_time, source_id,
                        /*append_only=*/false);
}

void OriginTrials::PersistAdditionalTrialsFromTokens(
    const url::Origin& origin,
    const url::Origin& partition_origin,
    base::span<const url::Origin> script_origins,
    const base::span<const std::string> header_tokens,
    const base::Time current_time,
    std::optional<ukm::SourceId> source_id) {
  PersistTokensInternal(origin, partition_origin, script_origins, header_tokens,
                        current_time, source_id,
                        /*append_only=*/true);
}

void OriginTrials::PersistTokensInternal(
    const url::Origin& origin,
    const url::Origin& partition_origin,
    base::span<const url::Origin> script_origins,
    const base::span<const std::string> header_tokens,
    const base::Time current_time,
    std::optional<ukm::SourceId> source_id,
    bool append_only) {
  if (origin.opaque()) {
    return;
  }

  base::flat_map<url::Origin, std::vector<blink::TrialToken>> valid_tokens;
  if (!append_only) {
    // Explicitly initialize the entry for the first-party origin since an empty
    // vector means tokens should be cleared if `append_only` is false.
    valid_tokens[origin] = {};
  }

  // Parse the provided tokens
  for (const std::string_view token : header_tokens) {
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
    // TODO(crbug.com/40189223): Should be part of general validation logic.
    if (!trial_token_validator_->TrialEnablesFeaturesForOS(
            parsed_token->feature_name())) {
      continue;
    }
    if (parsed_token->is_third_party()) {
      // TODO(crbug.com/40257643): Support for all third-party tokens.
      // Only accept deprecation trials as third-party for now.
      bool deprecation_trial = false;
      for (const blink::mojom::OriginTrialFeature feature :
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
      valid_tokens[parsed_token->origin()].push_back(std::move(*parsed_token));
    }
  }
  std::string partition_site = GetTokenPartitionSite(partition_origin);
  for (const auto& origin_token_pair : valid_tokens) {
    UpdatePersistedTokenSet(origin, origin_token_pair.first,
                            origin_token_pair.second, partition_site, source_id,
                            append_only);
  }
}

base::flat_set<std::string> OriginTrials::GetPersistedTrialsForOriginWithMatch(
    const url::Origin& origin,
    const url::Origin& partition_origin,
    const base::Time current_time,
    const std::optional<blink::mojom::OriginTrialFeature> trial_feature_match)
    const {
  if (origin.opaque()) {
    return {};
  }

  SiteOriginTrialTokens potential_tokens =
      persistence_provider_->GetPotentialPersistentTrialTokens(origin);

  base::flat_set<std::string> enabled_trials;
  for (const auto& [token_origin, saved_tokens] : potential_tokens) {
    for (const PersistedTrialToken& token : saved_tokens) {
      if (trial_feature_match &&
          // TODO(crbug.com/40189223): FeaturesEnabledByTrial should be part of
          // general validation logic.
          !base::Contains(
              trial_token_validator_->FeaturesEnabledByTrial(token.trial_name),
              trial_feature_match.value())) {
        continue;
      }
      if (!MatchesTokenOrigin(token_origin, token.match_subdomains, origin)) {
        continue;
      }

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
  NotifyPersistedTokensCleared();
}

// static
std::string OriginTrials::GetTokenPartitionSite(const url::Origin& origin) {
  if (origin.opaque()) {
    return kOpaqueOriginPartitionKey;
  }
  return net::SchemefulSite(origin).Serialize();
}

void OriginTrials::UpdatePersistedTokenSet(
    const url::Origin& document_origin,
    const url::Origin& token_origin,
    base::span<const blink::TrialToken> new_tokens,
    const std::string& partition_site,
    std::optional<ukm::SourceId> source_id,
    bool append_only) {
  if (append_only && new_tokens.empty()) {
    return;  // Nothing to do.
  }

  base::flat_set<PersistedTrialToken> token_set =
      persistence_provider_->GetPersistentTrialTokens(token_origin);

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
        // Tokens that match subdomains should only be removed when not included
        // by a document loaded from the token origin.
        if (!token.match_subdomains || (document_origin == token_origin)) {
          token.RemoveFromPartition(partition_site);
          NotifyStatusChange(
              token.trial_name,
              OriginTrialStatusChangeDetails(token_origin, partition_site,
                                             token.match_subdomains,
                                             /*enabled=*/false, source_id));
        }
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
      // The status of the trial for `origin` under `partition_site` (with this
      // token/signature) only changes if the token was not already logically in
      // the top-level partition of `partition_site`.
      // NOTE: This is because `found_token` can "match" `new_token` without
      // `found_token->partition_sites` containing `partition_site`.
      if (!found_token->partition_sites.contains(partition_site)) {
        NotifyStatusChange(
            found_token->trial_name,
            OriginTrialStatusChangeDetails(token_origin, partition_site,
                                           found_token->match_subdomains,
                                           /*enabled=*/true, source_id));
      }

      // Update the existing stored trial token with the metadata fields, as it
      // may be a newly issued token.
      found_token->AddToPartition(partition_site);
    } else {
      token_set.emplace(new_token, partition_site);
      NotifyStatusChange(
          new_token.feature_name(),
          OriginTrialStatusChangeDetails(token_origin, partition_site,
                                         new_token.match_subdomains(),
                                         /*enabled=*/true, source_id));
    }
  }
  persistence_provider_->SavePersistentTrialTokens(token_origin,
                                                   std::move(token_set));
}

}  // namespace origin_trials
