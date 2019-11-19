// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/ios/ukm_url_recorder.h"

#include <utility>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/web_state.h"
#include "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"
#include "services/metrics/public/cpp/delegating_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ukm {

namespace internal {

// SourceUrlRecorderWebStateObserver is responsible for recording UKM source
// URLs for all main frame navigations in a given WebState.
// SourceUrlRecorderWebStateObserver records both the final URL for a
// navigation and, if the navigation was redirected, the initial URL as well.
class SourceUrlRecorderWebStateObserver
    : public web::WebStateObserver,
      public web::WebStateUserData<SourceUrlRecorderWebStateObserver> {
 public:
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

  void MaybeRecordUrl(web::NavigationContext* navigation_context,
                      const GURL& initial_url);

  // Map from navigation ID to the initial URL for that navigation.
  base::flat_map<int64_t, GURL> pending_navigations_;

  SourceId last_committed_source_id_;

  WEB_STATE_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(SourceUrlRecorderWebStateObserver);
};

WEB_STATE_USER_DATA_KEY_IMPL(SourceUrlRecorderWebStateObserver)

SourceUrlRecorderWebStateObserver::SourceUrlRecorderWebStateObserver(
    web::WebState* web_state)
    : last_committed_source_id_(kInvalidSourceId) {
  web_state->AddObserver(this);
}

SourceUrlRecorderWebStateObserver::~SourceUrlRecorderWebStateObserver() {}

void SourceUrlRecorderWebStateObserver::WebStateDestroyed(
    web::WebState* web_state) {
  web_state->RemoveObserver(this);
}

void SourceUrlRecorderWebStateObserver::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  // NavigationContexts only exist for main frame navigations, so this will
  // never be called for non-main frame navigations and we don't need to filter
  // non-main frame navigations out here.

  // Additionally, at least for the time being, we don't track metrics for
  // same-document navigations (e.g. changes in URL fragment or URL changes
  // due to history.pushState) in UKM.
  if (navigation_context->IsSameDocument())
    return;

  // UKM doesn't want to record URLs for downloads. However, at the point a
  // navigation is started, we don't yet know if the navigation will result in a
  // download. Thus, we store the URL at the time a navigation was initiated
  // and only record it later, once we verify that the navigation didn't result
  // in a download.
  pending_navigations_.insert(std::make_pair(
      navigation_context->GetNavigationId(), navigation_context->GetUrl()));
}

void SourceUrlRecorderWebStateObserver::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  // NavigationContexts only exist for main frame navigations, so this will
  // never be called for non-main frame navigations and we don't need to filter
  // non-main frame navigations out here.

  auto it = pending_navigations_.find(navigation_context->GetNavigationId());
  if (it == pending_navigations_.end())
    return;

  DCHECK(!navigation_context->IsSameDocument());

  if (navigation_context->HasCommitted()) {
    last_committed_source_id_ = ConvertToSourceId(
        navigation_context->GetNavigationId(), SourceIdType::NAVIGATION_ID);
  }

  GURL initial_url = std::move(it->second);
  pending_navigations_.erase(it);

  // UKM doesn't want to record URLs for navigations that result in downloads.
  if (navigation_context->IsDownload())
    return;

  MaybeRecordUrl(navigation_context, initial_url);
}

SourceId SourceUrlRecorderWebStateObserver::GetLastCommittedSourceId() const {
  return last_committed_source_id_;
}

void SourceUrlRecorderWebStateObserver::MaybeRecordUrl(
    web::NavigationContext* navigation_context,
    const GURL& initial_url) {
  DCHECK(!navigation_context->IsSameDocument());

  DelegatingUkmRecorder* ukm_recorder = DelegatingUkmRecorder::Get();
  if (!ukm_recorder)
    return;

  const GURL& final_url = navigation_context->GetUrl();

  UkmSource::NavigationData navigation_data;
  // TODO(crbug.com/869123): This check isn't quite correct, as self redirecting
  // is possible. This may also be changed to include the entire redirect chain.
  if (final_url != initial_url)
    navigation_data.urls = {initial_url};
  navigation_data.urls.push_back(final_url);

  // TODO(crbug.com/873316): Fill out the other fields in NavigationData.

  const ukm::SourceId source_id = ukm::ConvertToSourceId(
      navigation_context->GetNavigationId(), ukm::SourceIdType::NAVIGATION_ID);
  ukm_recorder->RecordNavigation(source_id, navigation_data);
}

}  // namespace internal

void InitializeSourceUrlRecorderForWebState(web::WebState* web_state) {
  internal::SourceUrlRecorderWebStateObserver::CreateForWebState(web_state);
}

SourceId GetSourceIdForWebStateDocument(web::WebState* web_state) {
  internal::SourceUrlRecorderWebStateObserver* obs =
      internal::SourceUrlRecorderWebStateObserver::FromWebState(web_state);
  return obs ? obs->GetLastCommittedSourceId() : kInvalidSourceId;
}

}  // namespace ukm
