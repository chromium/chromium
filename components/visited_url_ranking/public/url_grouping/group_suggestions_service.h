// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VISITED_URL_RANKING_PUBLIC_URL_GROUPING_GROUP_SUGGESTIONS_SERVICE_H_
#define COMPONENTS_VISITED_URL_RANKING_PUBLIC_URL_GROUPING_GROUP_SUGGESTIONS_SERVICE_H_

#include "base/supports_user_data.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sessions/core/session_id.h"
#include "components/visited_url_ranking/public/url_grouping/group_suggestions.h"
#include "components/visited_url_ranking/public/url_grouping/group_suggestions_delegate.h"
#include "components/visited_url_ranking/public/url_grouping/tab_event_tracker.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace visited_url_ranking {

// Service for computing tab group suggestions.
class GroupSuggestionsService : public KeyedService,
                                public base::SupportsUserData {
 public:
#if BUILDFLAG(IS_ANDROID)
  // Returns a Java object of the type GroupSuggestionService for the given
  // GroupSuggestionService.
  static base::android::ScopedJavaLocalRef<jobject> GetJavaObject(
      GroupSuggestionsService* group_suggestion_service);
#endif  // BUILDFLAG(IS_ANDROID)

  GroupSuggestionsService() = default;
  ~GroupSuggestionsService() override = default;

  GroupSuggestionsService(const GroupSuggestionsService&) = delete;
  GroupSuggestionsService& operator=(const GroupSuggestionsService&) = delete;

  // Get the tab tracker for recording tab events.
  virtual TabEventTracker* GetTabEventTracker() = 0;

  // The scope of the suggestions that the UI / Delegate can handle. Eg: group
  // tabs within a window or tab model, or include synced tabs, or include
  // history, tab groups etc.
  struct Scope {
    // The ID of the tab model or the current session.
    SessionID tab_session_id = SessionID::InvalidValue();

    bool operator==(const Scope&) const = default;
    bool operator<(const Scope& o) const {
      return tab_session_id.id() < o.tab_session_id.id();
    }
  };

  // Returns the cached suggestions for the given `scope`.
  // Returns empty suggestions if there are no cached suggestions for the
  // `scope`.
  virtual std::optional<CachedSuggestions> GetCachedSuggestions(
      const Scope& scope) = 0;

  // Delegate can be registered when the window/activity is running and
  // suggestions can be shown.
  virtual void RegisterDelegate(GroupSuggestionsDelegate*, const Scope&) = 0;
  virtual void UnregisterDelegate(GroupSuggestionsDelegate*) = 0;

  // Set config for testing. `computation_delay` sets the delay to wait between
  // 2 runs.
  virtual void SetConfigForTesting(base::TimeDelta computation_delay) = 0;
};

}  // namespace visited_url_ranking

#endif  // COMPONENTS_VISITED_URL_RANKING_PUBLIC_URL_GROUPING_GROUP_SUGGESTIONS_SERVICE_H_
