// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/starter_heuristic.h"

#include <set>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "components/autofill_assistant/browser/features.h"
#include "components/autofill_assistant/browser/intent_strings.h"
#include "components/autofill_assistant/browser/url_utils.h"
#include "components/url_matcher/url_matcher_constants.h"

namespace autofill_assistant {

// UrlFilter dictionary. The URL condition set defining a specific intent's
// URL filter. See also components/url_matcher/url_matcher_factory.h
const char kHeuristicUrlConditionSetKey[] = "conditionSet";

StarterHeuristic::StarterHeuristic() = default;
StarterHeuristic::~StarterHeuristic() = default;

void StarterHeuristic::InitFromHeuristicConfigs(
    const std::vector<std::unique_ptr<StarterHeuristicConfig>>& configs,
    StarterPlatformDelegate* platform_delegate,
    content::BrowserContext* browser_context) {
  url_matcher_ = std::make_unique<url_matcher::URLMatcher>();
  matcher_id_to_config_map_.clear();

  url_matcher::URLMatcherConditionSet::Vector condition_sets;
  base::flat_map<base::MatcherStringPattern::ID, HeuristicConfigEntry> mapping;
  base::MatcherStringPattern::ID next_condition_set_id = 0;
  for (const auto& config : configs) {
    for (const auto& condition_set : config->GetConditionSetsForClientState(
             platform_delegate, browser_context)) {
      if (!condition_set.is_dict()) {
        LOG(ERROR) << "Invalid heuristic config: expected a dictionary for "
                      "each condition set, but got "
                   << base::Value::GetTypeName(condition_set.type());
        return;
      }
      auto* url_conditions = condition_set.FindKeyOfType(
          kHeuristicUrlConditionSetKey, base::Value::Type::DICTIONARY);
      if (!url_conditions) {
        VLOG(1) << "Condition dict did not contain a value for 'conditionSet'";
        return;
      }

      std::string error;
      condition_sets.emplace_back(
          url_matcher::URLMatcherFactory::CreateFromURLFilterDictionary(
              url_matcher_->condition_factory(), url_conditions->GetDict(),
              next_condition_set_id, &error));
      if (!error.empty()) {
        VLOG(1) << "Error parsing url conditions: " << error;
        return;
      }
      mapping.insert(
          std::make_pair(next_condition_set_id++,
                         HeuristicConfigEntry(config->GetIntent(),
                                              config->GetDenylistedDomains())));
    }
  }

  VLOG(2) << "Read " << condition_sets.size() << " condition sets from "
          << configs.size() << " configs.";
  url_matcher_->AddConditionSets(condition_sets);
  matcher_id_to_config_map_ = std::move(mapping);
}

bool StarterHeuristic::HasConditionSets() const {
  return !matcher_id_to_config_map_.empty();
}

base::flat_set<std::string> StarterHeuristic::IsHeuristicMatch(
    const GURL& url,
    base::flat_map<base::MatcherStringPattern::ID, HeuristicConfigEntry>
        copied_matcher_id_to_config_map) const {
  base::flat_set<std::string> matching_intents;
  if (copied_matcher_id_to_config_map.empty() || !url.is_valid()) {
    return matching_intents;
  }

  std::set<base::MatcherStringPattern::ID> matches =
      url_matcher_->MatchURL(url);
  for (const auto& match : matches) {
    auto config = copied_matcher_id_to_config_map.find(match);
    if (config == copied_matcher_id_to_config_map.end()) {
      DCHECK(false);
      continue;
    }
    // Skip matches if they are in the denylist of that config.
    if (config->second.denylisted_domains.count(
            url_utils::GetOrganizationIdentifyingDomain(url)) > 0) {
      continue;
    }
    matching_intents.emplace(config->second.intent);
  }
  return matching_intents;
}

void StarterHeuristic::RunHeuristicAsync(
    const GURL& url,
    base::OnceCallback<void(const base::flat_set<std::string>& intents)>
        callback) const {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&StarterHeuristic::IsHeuristicMatch,
                     base::RetainedRef(this), url, matcher_id_to_config_map_),
      std::move(callback));
}

StarterHeuristic::HeuristicConfigEntry::HeuristicConfigEntry(
    const std::string& _intent,
    const base::flat_set<std::string>& _denylisted_domains)
    : intent(_intent), denylisted_domains(_denylisted_domains) {}
StarterHeuristic::HeuristicConfigEntry::HeuristicConfigEntry(
    const HeuristicConfigEntry&) = default;
StarterHeuristic::HeuristicConfigEntry::~HeuristicConfigEntry() = default;

}  // namespace autofill_assistant
