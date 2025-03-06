// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VISITED_URL_RANKING_PUBLIC_URL_GROUPING_GROUP_SUGGESTIONS_SERVICE_H_
#define COMPONENTS_VISITED_URL_RANKING_PUBLIC_URL_GROUPING_GROUP_SUGGESTIONS_SERVICE_H_

#include "base/functional/callback_forward.h"
#include "components/sessions/core/session_id.h"
#include "components/visited_url_ranking/public/url_grouping/group_suggestions.h"
#include "components/visited_url_ranking/public/url_grouping/suggestions_delegate.h"
#include "components/visited_url_ranking/public/url_grouping/tab_event_tracker.h"

namespace visited_url_ranking {

// Service for computing tab group suggestions.
class GroupSuggestionsService {
 public:
  virtual ~GroupSuggestionsService() = 0;

  GroupSuggestionsService(const GroupSuggestionsService&) = delete;
  GroupSuggestionsService& operator=(const GroupSuggestionsService&) = delete;

  // Get the tab tracker for recording tab events.
  virtual TabEventTracker* GetTabEventTracker();

  // The scope of the suggestions that the UI / Delegate can handle. Eg: group
  // tabs within a window or tab model, or include synced tabs, or include
  // history, tab groups etc.
  struct Scope {
    // The ID of the tab model or the current session.
    SessionID tab_session_id;
  };

  // Delegate can be registered when the window/activity is running and
  // suggestions can be shown.
  virtual void RegisterDelegate(SuggestionsDelegate*, Scope) = 0;
  virtual void UnregisterDelegate(SuggestionsDelegate*) = 0;
};

}  // namespace visited_url_ranking

#endif  // COMPONENTS_VISITED_URL_RANKING_PUBLIC_URL_GROUPING_GROUP_SUGGESTIONS_SERVICE_H_
