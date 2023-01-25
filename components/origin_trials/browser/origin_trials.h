// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ORIGIN_TRIALS_BROWSER_ORIGIN_TRIALS_H_
#define COMPONENTS_ORIGIN_TRIALS_BROWSER_ORIGIN_TRIALS_H_

#include <memory>
#include <string>

#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/origin_trials/common/origin_trials_persistence_provider.h"
#include "components/origin_trials/common/persisted_trial_token.h"
#include "content/public/browser/origin_trials_controller_delegate.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/origin_trials/trial_token_validator.h"

namespace url {
class Origin;
}

namespace origin_trials {

// Implementation of persistent origin trials for the browser process.
//
// This class manages persistent origin trials, allowing the browser to check
// if a given trial is enabled or not.
//
// Persisting the enabled trials is handled by the |persistence_provider| passed
// in through the constructor.
class OriginTrials : public KeyedService,
                     public content::OriginTrialsControllerDelegate {
 public:
  OriginTrials(
      std::unique_ptr<OriginTrialsPersistenceProvider> persistence_provider,
      std::unique_ptr<blink::TrialTokenValidator> token_validator);

  OriginTrials(const OriginTrials&) = delete;
  OriginTrials(const OriginTrials&&) = delete;
  OriginTrials& operator=(const OriginTrials&) = delete;
  OriginTrials& operator=(const OriginTrials&&) = delete;

  ~OriginTrials() override;

  // content::OriginTrialsControllerDelegate
  void PersistTrialsFromTokens(
      const url::Origin& origin,
      const url::Origin& partition_origin,
      const base::span<const std::string> header_tokens,
      const base::Time current_time) override;
  bool IsTrialPersistedForOrigin(const url::Origin& origin,
                                 const url::Origin& partition_origin,
                                 const base::StringPiece trial_name,
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

  // Helper to return the still-valid persisted trials, with an optional
  // |trial_name_match| which can be passed to ensure we only validate
  // and return the trial if it matches the passed name.
  // If no |trial_name_match| is provided, it will return all persisted trials
  // that are still valid.
  base::flat_set<std::string> GetPersistedTrialsForOriginWithMatch(
      const url::Origin& origin,
      const url::Origin& partition_origin,
      const base::Time current_time,
      const absl::optional<const base::StringPiece> trial_name_match) const;

  // Update the passed |token_set| with the new tokens, partitioned by
  // |partition_site|. Performs the update directly on the passed |token_set|.
  static void UpdatePersistedTokenSet(
      base::flat_set<PersistedTrialToken>& token_set,
      std::vector<blink::TrialToken> new_tokens,
      std::string partition_site);

  // Get the 'site' used as the partitioning key for trial tokens.
  //
  // The key is the eTLD+1 of the |origin|, taking private registries such as
  // blogspot.com into account.
  static std::string GetTokenPartitionSite(const url::Origin& origin);
};

}  // namespace origin_trials

#endif  // COMPONENTS_ORIGIN_TRIALS_BROWSER_ORIGIN_TRIALS_H_
