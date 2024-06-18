// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UKM_IOS_UKM_URL_RECORDER_H_
#define COMPONENTS_UKM_IOS_UKM_URL_RECORDER_H_

#include <set>

#include "base/containers/flat_map.h"
#include "ios/web/public/navigation/navigation_context.h"
#include "ios/web/public/web_state.h"
#include "ios/web/public/web_state_observer.h"
#include "ios/web/public/web_state_user_data.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/gurl.h"

namespace ukm {
class UkmUrlRecorderTest;

namespace internal {

// SourceUrlRecorderWebStateObserver is responsible for recording UKM source
// URLs for all main frame navigations in a given WebState.
// SourceUrlRecorderWebStateObserver records both the final URL for a
// navigation and, if the navigation was redirected, the initial URL as well.
class SourceUrlRecorderWebStateObserver
    : public web::WebStateObserver,
      public web::WebStateUserData<SourceUrlRecorderWebStateObserver> {
 public:
  SourceUrlRecorderWebStateObserver(const SourceUrlRecorderWebStateObserver&) =
      delete;
  SourceUrlRecorderWebStateObserver& operator=(
      const SourceUrlRecorderWebStateObserver&) = delete;

  ~SourceUrlRecorderWebStateObserver() override;

  // web::WebStateObserver
  void DidStartNavigation(web::WebState* web_state,
                          web::NavigationContext* navigation_context) override;
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  SourceId GetLastCommittedSourceId() const;

 private:
  explicit SourceUrlRecorderWebStateObserver(web::WebState* web_state);
  friend class web::WebStateUserData<SourceUrlRecorderWebStateObserver>;
  friend class ukm::UkmUrlRecorderTest;

  void MaybeRecordUrl(web::NavigationContext* navigation_context,
                      const GURL& initial_url);

  // Map from navigation ID to the initial URL for that navigation.
  base::flat_map<int64_t, GURL> pending_navigations_;

  SourceId last_committed_source_id_;

  // All the navigation_ids that are already reported to the UKM recorder.
  std::set<int64_t> navigation_ids_seen_;

  WEB_STATE_USER_DATA_KEY_DECL();
};

}  // namespace internal

// Initializes recording of UKM source URLs for the given WebState.
void InitializeSourceUrlRecorderForWebState(web::WebState* web_state);

// Gets the observer instance for the given given WebState.
internal::SourceUrlRecorderWebStateObserver*
GetSourceUrlRecorderForWebStateForWebState(web::WebState* web_state);

// Gets a UKM SourceId for the currently committed document of web state.
// Returns kInvalidSourceId if no commit has been observed.
SourceId GetSourceIdForWebStateDocument(web::WebState* web_state);

}  // namespace ukm

#endif  // COMPONENTS_UKM_IOS_UKM_URL_RECORDER_H_
