// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ORIGIN_TRIALS_BROWSER_ORIGIN_TRIALS_H_
#define COMPONENTS_ORIGIN_TRIALS_BROWSER_ORIGIN_TRIALS_H_

#include <map>
#include <memory>
#include <optional>
#include <string>

#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/origin_trials/common/origin_trials_persistence_provider.h"
#include "content/public/browser/origin_trial_status_change_details.h"
#include "content/public/browser/origin_trials_controller_delegate.h"
#include "third_party/blink/public/common/origin_trials/trial_token_validator.h"
#include "third_party/blink/public/mojom/origin_trial_feature/origin_trial_feature.mojom-shared.h"

using content::OriginTrialStatusChangeDetails;

namespace url {
class Origin;
}

namespace origin_trials {

// Implementation of persistent origin trials for the browser process.
//
// This class manages persistent origin trials, allowing the browser to check
// if a given trial is enabled or not.
//
// Persisting the enabled trials is handled by the `persistence_provider` passed
// in through the constructor.
class OriginTrials : public KeyedService,
                     public content::OriginTrialsControllerDelegate {
 public:
  using ObserverMap = std::map<
      std::string,
      base::ObserverList<content::OriginTrialsControllerDelegate::Observer>>;

  OriginTrials(
      std::unique_ptr<OriginTrialsPersistenceProvider> persistence_provider,
      std::unique_ptr<blink::TrialTokenValidator> token_validator);

  OriginTrials(const OriginTrials&) = delete;
  OriginTrials(const OriginTrials&&) = delete;
  OriginTrials& operator=(const OriginTrials&) = delete;
  OriginTrials& operator=(const OriginTrials&&) = delete;

  ~OriginTrials() override;

  // content::OriginTrialsControllerDelegate
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void PersistTrialsFromTokens(
      const url::Origin& origin,
      const url::Origin& partition_origin,
      const base::span<const std::string> header_tokens,
      const base::Time current_time,
      std::optional<ukm::SourceId> source_id) override;
  void PersistAdditionalTrialsFromTokens(
      const url::Origin& origin,
      const url::Origin& partition_origin,
      base::span<const url::Origin> script_origins,
      const base::span<const std::string> header_tokens,
      const base::Time current_time,
      std::optional<ukm::SourceId> source_id) override;
  bool IsFeaturePersistedForOrigin(const url::Origin& origin,
                                   const url::Origin& partition_origin,
                                   blink::mojom::OriginTrialFeature feature,
                                   const base::Time current_time) override;
  base::flat_set<std::string> GetPersistedTrialsForOrigin(
      const url::Origin& origin,
      const url::Origin& partition_origin,
      base::Time current_time) override;
  void ClearPersistedTokens() override;

 private:
  friend class OriginTrialsTest;
  std::unique_ptr<OriginTrialsPersistenceProvider> persistence_provider_;
  std::unique_ptr<blink::TrialTokenValidator> trial_token_validator_;
  ObserverMap observer_map_;

  void NotifyStatusChange(const std::string& trial,
                          const OriginTrialStatusChangeDetails& details);
  void NotifyPersistedTokensCleared();

  // Returns true if `origin` can use a token made for `token_origin`. For
  // `origin` to use `token_origin`'s token, at least one of the following must
  // be true:
  //  - `origin` equals `token_origin`
  //  - `match_subdomains` is true and `origin` is a subdomain of `token_origin`
  // NOTE: This is meant to mirror the logic used in
  // `blink::TrialToken::ValidateOrigin()`.
  // TODO(crbug.com/40189223): Find a way to share/reuse the logic in
  // `blink::TrialToken`. Otherwise, the logic could change in one place and not
  // the other.
  bool MatchesTokenOrigin(const url::Origin& token_origin,
                          bool match_subdomains,
                          const url::Origin& origin) const;

  void PersistTokensInternal(const url::Origin& origin,
                             const url::Origin& partition_origin,
                             base::span<const url::Origin> script_origins,
                             const base::span<const std::string> header_tokens,
                             const base::Time current_time,
                             std::optional<ukm::SourceId> source_id,
                             bool append_only);

  // Helper to return the still-valid persisted trials, with an optional
  // `trial_feature_match` which can be passed to ensure we only validate
  // and return the trial if it enables the desired trial feature.
  // If no `trial_feature_match` is provided, it will return all persisted
  // trials that are still valid.
  base::flat_set<std::string> GetPersistedTrialsForOriginWithMatch(
      const url::Origin& origin,
      const url::Origin& partition_origin,
      const base::Time current_time,
      const std::optional<blink::mojom::OriginTrialFeature> trial_feature_match)
      const;

  // Update the stored tokens for `token_origin` with the `new_tokens`,
  // partitioned by `partition_site`.
  // If `append_only` is set to true, existing tokens for `token_origin` that
  // aren't found in `new_tokens` will be cleared, unless they match subdomains,
  // in which case they will only be cleared if `document_origin` equals
  // `token_origin`.
  void UpdatePersistedTokenSet(const url::Origin& document_origin,
                               const url::Origin& token_origin,
                               base::span<const blink::TrialToken> new_tokens,
                               const std::string& partition_site,
                               std::optional<ukm::SourceId> source_id,
                               bool append_only);

  // Get the 'site' used as the partitioning key for trial tokens.
  //
  // The key is the eTLD+1 of the `origin`, taking private registries such as
  // blogspot.com into account.
  static std::string GetTokenPartitionSite(const url::Origin& origin);
};

}  // namespace origin_trials

#endif  // COMPONENTS_ORIGIN_TRIALS_BROWSER_ORIGIN_TRIALS_H_
