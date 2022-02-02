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

// String parameter containing the JSON-encoded parameter dictionary.
const char kJsonParameterDictKey[] = "json_parameters";

constexpr base::FeatureParam<std::string> kFieldTrialParams{
    &features::kAutofillAssistantUrlHeuristics, kJsonParameterDictKey, ""};

// Array of strings, containing the list of globally denylisted domains.
const char kDenylistedDomainsKey[] = "denylistedDomains";
// Array of heuristics, each with its own intent and conditions.
const char kHeuristicsKey[] = "heuristics";
// String. The intent associated with a specific heuristic.
const char kHeuristicIntentKey[] = "intent";
// UrlFilter dictionary. The URL condition set defining a specific intent's
// heuristic. See also components/url_matcher/url_matcher_factory.h
const char kHeuristicUrlConditionSetKey[] = "conditionSet";

StarterHeuristic::StarterHeuristic() {
  InitFromTrialParams();
}

StarterHeuristic::~StarterHeuristic() = default;

void StarterHeuristic::InitFromTrialParams() {
  DCHECK(matcher_id_to_intent_map_.empty())
      << "Called after already initialized";

  std::string parameters = kFieldTrialParams.Get();
  if (parameters.empty()) {
    VLOG(2) << "Field trial parameter not set";
    return;
  }
  absl::optional<base::Value> dict = base::JSONReader::Read(parameters);
  if (!dict || !dict->is_dict()) {
    VLOG(1) << "Failed to parse field trial params as JSON object: "
            << parameters;
    if (VLOG_IS_ON(1)) {
      auto err = base::JSONReader::ReadAndReturnValueWithError(parameters);
      VLOG(1) << err.error_message << ", line: " << err.error_line
              << ", col: " << err.error_column;
    }
    return;
  }

  // Read mandatory list of heuristics.
  auto* heuristics = dict->FindListKey(kHeuristicsKey);
  if (heuristics == nullptr || !heuristics->is_list()) {
    VLOG(1) << "Field trial params did not contain heuristics";
    return;
  }
  url_matcher::URLMatcherConditionSet::Vector condition_sets;
  base::flat_map<url_matcher::URLMatcherConditionSet::ID, std::string> mapping;
  url_matcher::URLMatcherConditionSet::ID next_condition_set_id = 0;
  for (const auto& heuristic : heuristics->GetListDeprecated()) {
    auto* intent =
        heuristic.FindKeyOfType(kHeuristicIntentKey, base::Value::Type::STRING);
    auto* url_conditions = heuristic.FindKeyOfType(
        kHeuristicUrlConditionSetKey, base::Value::Type::DICTIONARY);
    if (!intent || !url_conditions) {
      VLOG(1) << "Heuristic did not contain intent or url_conditions";
      return;
    }

    std::string error;
    const auto& url_conditions_dict =
        base::Value::AsDictionaryValue(*url_conditions);
    condition_sets.emplace_back(
        url_matcher::URLMatcherFactory::CreateFromURLFilterDictionary(
            url_matcher_.condition_factory(), &url_conditions_dict,
            next_condition_set_id, &error));
    if (!error.empty()) {
      VLOG(1) << "Error pasing url conditions: " << error;
      return;
    }
    mapping[next_condition_set_id++] = *intent->GetIfString();
  }

  // Read optional list of denylisted domains.
  auto* denylisted_domains_value = dict->FindListKey(kDenylistedDomainsKey);
  base::flat_set<std::string> denylisted_domains;
  if (denylisted_domains_value != nullptr) {
    for (const auto& domain : denylisted_domains_value->GetListDeprecated()) {
      if (!domain.is_string()) {
        VLOG(1) << "Invalid type for denylisted domain";
        return;
      }
      denylisted_domains.insert(*domain.GetIfString());
    }
  }

  VLOG(2) << "Read " << condition_sets.size() << " condition sets and "
          << denylisted_domains.size() << " denylisted domains.";
  denylisted_domains_ = std::move(denylisted_domains);
  url_matcher_.AddConditionSets(condition_sets);
  matcher_id_to_intent_map_ = std::move(mapping);
}

base::flat_set<std::string> StarterHeuristic::IsHeuristicMatch(
    const GURL& url) const {
  base::flat_set<std::string> matching_intents;
  if (matcher_id_to_intent_map_.empty() || !url.is_valid()) {
    return matching_intents;
  }

  if (denylisted_domains_.count(
          url_utils::GetOrganizationIdentifyingDomain(url)) > 0) {
    return matching_intents;
  }

  std::set<url_matcher::URLMatcherConditionSet::ID> matches =
      url_matcher_.MatchURL(url);
  for (const auto& match : matches) {
    auto intent = matcher_id_to_intent_map_.find(match);
    if (intent == matcher_id_to_intent_map_.end()) {
      DCHECK(false);
      continue;
    }
    matching_intents.emplace(intent->second);
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
                     base::RetainedRef(this), url),
      std::move(callback));
}

}  // namespace autofill_assistant
