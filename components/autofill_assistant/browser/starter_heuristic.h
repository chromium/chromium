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
  ~StarterHeuristic();

  // Initializes the heuristic from the heuristic trial parameters. If there is
  // no trial or parsing fails, the heuristic will be empty and as such always
  // report absl::nullopt. However, if you want to disable implicit startup,
  // you should disable the dedicated in-CCT and/or in-Tab triggering trials
  // instead to prevent the heuristic from being called in the first place.
  void InitFromTrialParams();

  // Runs the heuristic against |url|. Returns all matching intents.
  base::flat_set<std::string> IsHeuristicMatch(const GURL& url) const;

  // The set of denylisted domains that will always return false before
  // considering any of the intent heuristics.
  base::flat_set<std::string> denylisted_domains_;

  // The URL matcher containing one URLMatcherConditionSet per supported intent.
  url_matcher::URLMatcher url_matcher_;

  // Arbitrary mapping of matcher IDs to intent strings. This mapping is built
  // dynamically to allow the heuristic to work on intents that are otherwise
  // unknown to the client.
  base::flat_map<base::MatcherStringPattern::ID, std::string>
      matcher_id_to_intent_map_;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_STARTER_HEURISTIC_H_
