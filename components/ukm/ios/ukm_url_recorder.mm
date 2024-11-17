// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/ios/ukm_url_recorder.h"

#include <set>
#include <utility>

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "ios/web/public/navigation/navigation_context.h"
#include "ios/web/public/web_state.h"
#include "ios/web/public/web_state_observer.h"
#include "ios/web/public/web_state_user_data.h"
#include "services/metrics/public/cpp/delegating_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/gurl.h"

namespace ukm {

namespace internal {

SourceUrlRecorderWebStateObserver::SourceUrlRecorderWebStateObserver(
    web::WebState* web_state)
    : last_committed_source_id_(kInvalidSourceId) {
  web_state->AddObserver(this);
}

SourceUrlRecorderWebStateObserver::~SourceUrlRecorderWebStateObserver() =
    default;

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
  const int64_t navigation_id = navigation_context->GetNavigationId();

  DelegatingUkmRecorder* ukm_recorder = DelegatingUkmRecorder::Get();
  if (!ukm_recorder)
    return;

  // Check to avoid recording the same navigation more than once in the UKM
  // recorder. On iOS, the logic to infer navigation from the limited signals
  // that WebKit is imperfect, since we don't get all the usual WebKit
  // navigation callbacks on JS-initiated same-document navigations.
  // For example, suppose a sequence visits is
  // 1. https://example.com#foo
  // 2. https://example.com#bar
  // 3. https://example.com#foo
  // The first and third visits to the same URL are erroneously considered the
  // same navigation, thus the NavigationContext from the first is reused. In
  // contrast, UKM considers all of these three different navigations, in line
  // with the logic onother platforms. As a workaround here, we skip recording
  // the last visit to #foo to UKM. Cases such as these are observed to be very
  // rare.
  if (base::Contains(navigation_ids_seen_, navigation_id)) {
    return;
  }

  const GURL& final_url = navigation_context->GetUrl();

  UkmSource::NavigationData navigation_data;
  // TODO(crbug.com/40587196): This check isn't quite correct, as self
  // redirecting is possible. This may also be changed to include the entire
  // redirect chain.
  if (final_url != initial_url)
    navigation_data.urls = {initial_url};
  navigation_data.urls.push_back(final_url);

  // TODO(crbug.com/41407501): Fill out the other fields in NavigationData.

  navigation_ids_seen_.insert(navigation_id);
  const ukm::SourceId source_id =
      ukm::ConvertToSourceId(navigation_id, ukm::SourceIdType::NAVIGATION_ID);
  ukm_recorder->RecordNavigation(source_id, navigation_data);
}

WEB_STATE_USER_DATA_KEY_IMPL(SourceUrlRecorderWebStateObserver)

}  // namespace internal

void InitializeSourceUrlRecorderForWebState(web::WebState* web_state) {
  internal::SourceUrlRecorderWebStateObserver::CreateForWebState(web_state);
}

internal::SourceUrlRecorderWebStateObserver*
GetSourceUrlRecorderForWebStateForWebState(web::WebState* web_state) {
  return internal::SourceUrlRecorderWebStateObserver::FromWebState(web_state);
}

SourceId GetSourceIdForWebStateDocument(web::WebState* web_state) {
  auto* obs = GetSourceUrlRecorderForWebStateForWebState(web_state);
  return obs ? obs->GetLastCommittedSourceId() : kInvalidSourceId;
}

}  // namespace ukm
