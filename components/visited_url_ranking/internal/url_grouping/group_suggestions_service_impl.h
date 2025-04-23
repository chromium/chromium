// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VISITED_URL_RANKING_INTERNAL_URL_GROUPING_GROUP_SUGGESTIONS_SERVICE_IMPL_H_
#define COMPONENTS_VISITED_URL_RANKING_INTERNAL_URL_GROUPING_GROUP_SUGGESTIONS_SERVICE_IMPL_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "base/time/clock.h"
#include "components/visited_url_ranking/internal/url_grouping/group_suggestions_manager.h"
#include "components/visited_url_ranking/internal/url_grouping/tab_event_tracker_impl.h"
#include "components/visited_url_ranking/internal/url_grouping/tab_events_visit_transformer.h"
#include "components/visited_url_ranking/public/url_grouping/group_suggestions_service.h"
#include "components/visited_url_ranking/public/url_grouping/tab_event_tracker.h"
#include "components/visited_url_ranking/public/visited_url_ranking_service.h"

class PrefService;
class PrefRegistrySimple;

namespace visited_url_ranking {

class GroupSuggestionsServiceImpl : public GroupSuggestionsService,
                                    public base::SupportsUserData::Data {
 public:
  GroupSuggestionsServiceImpl(
      VisitedURLRankingService* visited_url_ranking_service,
      TabEventsVisitTransformer* tab_events_transformer,
      PrefService* pref_service);
  ~GroupSuggestionsServiceImpl() override;

  GroupSuggestionsServiceImpl(const GroupSuggestionsServiceImpl&) = delete;
  GroupSuggestionsServiceImpl& operator=(const GroupSuggestionsServiceImpl&) =
      delete;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // GroupSuggestionsService impl:
  TabEventTracker* GetTabEventTracker() override;
  void RegisterDelegate(GroupSuggestionsDelegate* delegate,
                        const Scope& scope) override;
  void UnregisterDelegate(GroupSuggestionsDelegate* delegate) override;
  void SetConfigForTesting(base::TimeDelta computation_delay) override;

  GroupSuggestionsManager* group_suggestions_manager_for_testing() {
    return group_suggestions_manager_.get();
  }

 private:
  void OnNewSuggestionTabEvent();

  const raw_ptr<VisitedURLRankingService> visited_url_ranking_service_;
  const raw_ptr<TabEventsVisitTransformer> tab_events_transformer_;
  std::unique_ptr<GroupSuggestionsManager> group_suggestions_manager_;
  std::unique_ptr<TabEventTrackerImpl> tab_tracker_;

  base::WeakPtrFactory<GroupSuggestionsServiceImpl> weak_ptr_factory_{this};
};

}  // namespace visited_url_ranking

#endif  // COMPONENTS_VISITED_URL_RANKING_INTERNAL_URL_GROUPING_GROUP_SUGGESTIONS_SERVICE_IMPL_H_
