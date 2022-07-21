// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_STARTER_HEURISTIC_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_STARTER_HEURISTIC_H_

#include <string>

#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/ref_counted.h"
#include "components/autofill_assistant/browser/starter_heuristic_configs/starter_heuristic_config.h"
#include "components/autofill_assistant/browser/starter_platform_delegate.h"
#include "components/url_matcher/url_matcher.h"
#include "components/url_matcher/url_matcher_factory.h"
#include "url/gurl.h"

namespace autofill_assistant {

// Utility that implements a heuristic for autofill-assistant URLs.
//
// This class inherits from RefCountedThreadSafe to allow safe evaluation on
// worker threads.
class StarterHeuristic : public base::RefCountedThreadSafe<StarterHeuristic> {
 public:
  StarterHeuristic();
  StarterHeuristic(const StarterHeuristic&) = delete;
  StarterHeuristic& operator=(const StarterHeuristic&) = delete;

  // (Re-)initializes this starter heuristic from the given set of configs and
  // the current client state.
  void InitFromHeuristicConfigs(
      const std::vector<std::unique_ptr<StarterHeuristicConfig>>& configs,
      StarterPlatformDelegate* platform_delegate);

  // Returns true if at least one condition set is available. There is no point
  // in running the heuristic otherwise.
  bool HasConditionSets() const;

  // Runs the heuristic against |url| and invokes the callback with all matching
  // intents.
  //
  // Note that this method runs on a worker thread, not on the caller's thread.
  // The callback will be invoked on the caller's sequence.
  void RunHeuristicAsync(
      const GURL& url,
      base::OnceCallback<void(const base::flat_set<std::string>& intents)>
          callback) const;

 private:
  friend class base::RefCountedThreadSafe<StarterHeuristic>;
  friend class StarterHeuristicTest;

  // Corresponds to a particular heuristic config. Used to map URL matcher IDs
  // to the originating heuristic config without having to take ownership of
  // or otherwise directly interacting with those configs.
  struct HeuristicConfigEntry {
    HeuristicConfigEntry(const std::string& intent,
                         const base::flat_set<std::string>& denylisted_domains);
    HeuristicConfigEntry(const HeuristicConfigEntry&);
    ~HeuristicConfigEntry();
    std::string intent;
    base::flat_set<std::string> denylisted_domains;
  };

  ~StarterHeuristic();

  // Initializes the heuristic from the heuristic trial parameters. If there is
  // no trial or parsing fails, the heuristic will be empty and as such always
  // report absl::nullopt. However, if you want to disable implicit startup,
  // you should disable the dedicated in-CCT and/or in-Tab triggering trials
  // instead to prevent the heuristic from being called in the first place.
  void InitFromTrialParams();

  // Runs the heuristic against |url|. Returns all matching intents.
  base::flat_set<std::string> IsHeuristicMatch(
      const GURL& url,
      base::flat_map<base::MatcherStringPattern::ID, HeuristicConfigEntry>
          copied_matcher_id_to_config_map) const;

  // The URL matcher containing one URLMatcherConditionSet per supported intent.
  std::unique_ptr<url_matcher::URLMatcher> url_matcher_;

  // Arbitrary mapping of matcher IDs to heuristic configs.
  base::flat_map<base::MatcherStringPattern::ID, HeuristicConfigEntry>
      matcher_id_to_config_map_;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_STARTER_HEURISTIC_H_
