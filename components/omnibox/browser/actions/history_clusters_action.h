// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_ACTIONS_HISTORY_CLUSTERS_ACTION_H_
#define COMPONENTS_OMNIBOX_BROWSER_ACTIONS_HISTORY_CLUSTERS_ACTION_H_

#include "build/build_config.h"
#include "components/history/core/browser/history_types.h"
#include "components/omnibox/browser/actions/omnibox_action.h"

struct AutocompleteMatch;
class AutocompleteResult;

namespace gfx {
struct VectorIcon;
}

namespace history_clusters {

class HistoryClustersService;

// Helper for `TopRelevance()` to look at a subset of matches.
enum class TopRelevanceFilter : int {
  FILTER_FOR_SEARCH_MATCHES,
  FILTER_FOR_NON_SEARCH_MATCHES
};

// Find the top relevance of either search or navigation matches. Returns 0 if
// there are no search or navigation matches.
int TopRelevance(std::vector<AutocompleteMatch>::const_iterator matches_begin,
                 std::vector<AutocompleteMatch>::const_iterator matches_end,
                 TopRelevanceFilter filter);

// Return if the history cluster action or suggestion should be excluded due to
// matches indicating a nav-intent input. Should only be called if nav-intent
// filtering is enabled to avoid extra computations.
bool IsNavigationIntent(int top_search_relevance,
                        int top_navigation_relevance,
                        int navigation_intent_score_threshold);

// Gets the Journeys WebUI URL for `query`, i.e. chrome://history/journeys?q=%s.
GURL GetFullJourneysUrlForQuery(const std::string& query);

// Made public for testing.
class HistoryClustersAction : public OmniboxAction {
 public:
  HistoryClustersAction(
      const std::string& query,
      const history::ClusterKeywordData& matched_keyword_data);

  void RecordActionShown(size_t position, bool executed) const override;
  void Execute(ExecutionContext& context) const override;
  OmniboxActionId ActionId() const override;
#if defined(SUPPORT_PEDALS_VECTOR_ICONS)
  const gfx::VectorIcon& GetVectorIcon() const override;
#endif

 private:
  ~HistoryClustersAction() override;

  // Additional data of the matching keyword from the history clustering
  // service.
  history::ClusterKeywordData matched_keyword_data_;

  // Used to open journeys in side panel with relevant clusters
  std::string query_;
};

// If the feature is enabled, attaches any necessary History Clusters actions
// onto any relevant matches in `result`.
void AttachHistoryClustersActions(
    history_clusters::HistoryClustersService* service,
    AutocompleteResult& result);

}  // namespace history_clusters

#endif  // COMPONENTS_OMNIBOX_BROWSER_ACTIONS_HISTORY_CLUSTERS_ACTION_H_
