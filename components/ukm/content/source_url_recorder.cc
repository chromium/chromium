// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/content/source_url_recorder.h"

#include <utility>

#include "base/containers/flat_map.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial_params.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "services/metrics/public/cpp/delegating_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/gurl.h"

namespace ukm {

namespace internal {

int64_t CreateUniqueTabId() {
  static int64_t unique_id_counter = 0;
  return ++unique_id_counter;
}

// SourceUrlRecorderWebContentsObserver is responsible for recording UKM source
// URLs, for all (any only) main frame navigations in a given WebContents.
// SourceUrlRecorderWebContentsObserver records both the final URL for a
// navigation, and, if the navigation was redirected, the initial URL as well.
class SourceUrlRecorderWebContentsObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<
          SourceUrlRecorderWebContentsObserver> {
 public:
  SourceUrlRecorderWebContentsObserver(
      const SourceUrlRecorderWebContentsObserver&) = delete;
  SourceUrlRecorderWebContentsObserver& operator=(
      const SourceUrlRecorderWebContentsObserver&) = delete;

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidOpenRequestedURL(content::WebContents* new_contents,
                           content::RenderFrameHost* source_render_frame_host,
                           const GURL& url,
                           const content::Referrer& referrer,
                           WindowOpenDisposition disposition,
                           ui::PageTransition transition,
                           bool started_from_context_menu,
                           bool renderer_initiated) override;
  void WebContentsDestroyed() override;

  ukm::SourceId GetLastCommittedSourceId() const;
  ukm::SourceId GetLastCommittedFullNavigationOrSameDocumentSourceId() const;

 private:
  explicit SourceUrlRecorderWebContentsObserver(
      content::WebContents* web_contents);
  friend class content::WebContentsUserData<
      SourceUrlRecorderWebContentsObserver>;

  void HandleSameDocumentNavigation(
      content::NavigationHandle* navigation_handle);
  void HandleDifferentDocumentNavigation(
      content::NavigationHandle* navigation_handle,
      const GURL& initial_url);

  void MaybeRecordUrl(content::NavigationHandle* navigation_handle,
                      const GURL& initial_url);

  // Whether URLs should be recorded in UKM Sources.
  bool ShouldRecordURLs() const;

  // Map from navigation ID to the initial URL for that navigation.
  base::flat_map<int64_t, GURL> pending_navigations_;

  // The source id of the last committed full navigation (where a full
  // navigation is a non-same-document navigation).
  SourceId last_committed_full_navigation_source_id_;

  // The source id of the last committed navigation, either full navigation or
  // same document.
  SourceId last_committed_full_navigation_or_same_document_source_id_;

  // The source id of the last committed source in the tab that opened this tab.
  // Will be set to kInvalidSourceId after the first navigation in this tab is
  // finished.
  SourceId opener_source_id_;

  const int64_t tab_id_;

  int num_same_document_sources_for_full_navigation_source_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(SourceUrlRecorderWebContentsObserver);

SourceUrlRecorderWebContentsObserver::SourceUrlRecorderWebContentsObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<SourceUrlRecorderWebContentsObserver>(
          *web_contents),
      last_committed_full_navigation_source_id_(ukm::kInvalidSourceId),
      last_committed_full_navigation_or_same_document_source_id_(
          ukm::kInvalidSourceId),
      opener_source_id_(ukm::kInvalidSourceId),
      tab_id_(CreateUniqueTabId()),
      num_same_document_sources_for_full_navigation_source_(0) {}

bool SourceUrlRecorderWebContentsObserver::ShouldRecordURLs() const {
  // TODO(crbug.com/40689292): ensure we only record URLs for tabs in a tab
  // strip.

  // If there is an outer WebContents, then this WebContents is embedded into
  // another one (e.g it is a portal or a Chrome App <webview>).
  return web_contents()->GetOuterWebContents() == nullptr;
}

void SourceUrlRecorderWebContentsObserver::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  // UKM only records URLs for main frame (web page) navigations, so ignore
  // non-main frame navs. Additionally, at least for the time being, we don't
  // track metrics for same-document navigations (e.g. changes in URL fragment,
  // or URL changes due to history.pushState) in UKM.
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  // UKM doesn't want to record URLs for downloads. However, at the point a
  // navigation is started, we don't yet know if the navigation will result in a
  // download. Thus, we record the URL at the time a navigation was initiated,
  // and only record it later, once we verify that the navigation didn't result
  // in a download.
  pending_navigations_.insert(std::make_pair(
      navigation_handle->GetNavigationId(), navigation_handle->GetURL()));
}

void SourceUrlRecorderWebContentsObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  auto it = pending_navigations_.find(navigation_handle->GetNavigationId());
  if (!navigation_handle->IsInPrimaryMainFrame()) {
    DCHECK(it == pending_navigations_.end());
    return;
  }

  if (navigation_handle->IsSameDocument()) {
    DCHECK(it == pending_navigations_.end());
    HandleSameDocumentNavigation(navigation_handle);
    return;
  }

  if (it != pending_navigations_.end()) {
    GURL initial_url = std::move(it->second);
    pending_navigations_.erase(it);
    HandleDifferentDocumentNavigation(navigation_handle, initial_url);
  }
}

void SourceUrlRecorderWebContentsObserver::HandleSameDocumentNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->HasCommitted())
    return;

  // Only record same-document sources if we were also recording the associated
  // full source.
  if (last_committed_full_navigation_source_id_ == ukm::kInvalidSourceId) {
    return;
  }

  // Since the navigation has committed, inform the UKM recorder that the
  // previous same-document source (if applicable) is no longer needed to be
  // kept alive in memory since we had navigated away. If the previous
  // navigation was a full navigation, we do not mark its source id since events
  // could be continued to be reported for it until the next full navigation
  // source is committed.
  ukm::DelegatingUkmRecorder* ukm_recorder = ukm::DelegatingUkmRecorder::Get();
  if (ukm_recorder &&
      GetLastCommittedSourceId() !=
          GetLastCommittedFullNavigationOrSameDocumentSourceId()) {
    ukm_recorder->MarkSourceForDeletion(
        GetLastCommittedFullNavigationOrSameDocumentSourceId());
  }

  MaybeRecordUrl(navigation_handle, GURL());

  last_committed_full_navigation_or_same_document_source_id_ =
      ukm::ConvertToSourceId(navigation_handle->GetNavigationId(),
                             ukm::SourceIdType::NAVIGATION_ID);
  ++num_same_document_sources_for_full_navigation_source_;
}

void SourceUrlRecorderWebContentsObserver::HandleDifferentDocumentNavigation(
    content::NavigationHandle* navigation_handle,
    const GURL& initial_url) {
  // UKM doesn't want to record URLs for navigations that result in downloads.
  if (navigation_handle->IsDownload())
    return;

  // If a new full navigation has been committed, there will be no more events
  // associated with previous navigation sources, so we mark them as obsolete.
  ukm::DelegatingUkmRecorder* ukm_recorder = ukm::DelegatingUkmRecorder::Get();
  if (navigation_handle->HasCommitted() && ukm_recorder) {
    // Source id of the previous full navigation.
    ukm_recorder->MarkSourceForDeletion(GetLastCommittedSourceId());
    // Source id of the previous navigation. If the previous navigation is a
    // full navigation, marking it again has no additional effect.
    ukm_recorder->MarkSourceForDeletion(
        GetLastCommittedFullNavigationOrSameDocumentSourceId());
  }

  MaybeRecordUrl(navigation_handle, initial_url);

  if (navigation_handle->HasCommitted()) {
    last_committed_full_navigation_source_id_ = ukm::ConvertToSourceId(
        navigation_handle->GetNavigationId(), ukm::SourceIdType::NAVIGATION_ID);
    last_committed_full_navigation_or_same_document_source_id_ =
        last_committed_full_navigation_source_id_;
    num_same_document_sources_for_full_navigation_source_ = 0;
  }

  // Reset the opener source id. Only the first source in a tab should have an
  // opener.
  opener_source_id_ = kInvalidSourceId;
}

void SourceUrlRecorderWebContentsObserver::DidOpenRequestedURL(
    content::WebContents* new_contents,
    content::RenderFrameHost* source_render_frame_host,
    const GURL& url,
    const content::Referrer& referrer,
    WindowOpenDisposition disposition,
    ui::PageTransition transition,
    bool started_from_context_menu,
    bool renderer_initiated) {
  // Ensure that a source recorder exists at this point, since it is possible
  // that this is called before tab helpers are added in //chrome, especially on
  // Android. See crbug.com/1024952 for more details.
  InitializeSourceUrlRecorderForWebContents(new_contents);
  SourceUrlRecorderWebContentsObserver::FromWebContents(new_contents)
      ->opener_source_id_ = GetLastCommittedSourceId();
}

void SourceUrlRecorderWebContentsObserver::WebContentsDestroyed() {
  // Inform the UKM recorder that the previous source is no longer needed to
  // be kept alive in memory since the tab has been closed or discarded. In case
  // of same-document navigation, a new source id would have been created
  // similarly to full-navigation, thus we are marking the last committed source
  // id regardless of which case it came from.
  ukm::DelegatingUkmRecorder* ukm_recorder = ukm::DelegatingUkmRecorder::Get();
  if (ukm_recorder) {
    ukm_recorder->MarkSourceForDeletion(
        GetLastCommittedFullNavigationOrSameDocumentSourceId());
  }
}

ukm::SourceId SourceUrlRecorderWebContentsObserver::GetLastCommittedSourceId()
    const {
  return last_committed_full_navigation_source_id_;
}

ukm::SourceId SourceUrlRecorderWebContentsObserver::
    GetLastCommittedFullNavigationOrSameDocumentSourceId() const {
  return last_committed_full_navigation_or_same_document_source_id_;
}

void SourceUrlRecorderWebContentsObserver::MaybeRecordUrl(
    content::NavigationHandle* navigation_handle,
    const GURL& initial_url) {
  DCHECK(navigation_handle->IsInPrimaryMainFrame());

  // TODO(crbug.com/40689295): If ShouldRecordURLs is false, we should still
  // create a UKM source, but not add any URLs to it.
  if (!ShouldRecordURLs())
    return;

  ukm::DelegatingUkmRecorder* ukm_recorder = ukm::DelegatingUkmRecorder::Get();
  if (!ukm_recorder)
    return;

  UkmSource::NavigationData navigation_data;
  const GURL& final_url = navigation_handle->GetURL();
  // TODO(crbug.com/40587196): This check isn't quite correct, as self
  // redirecting is possible. This may also be changed to include the entire
  // redirect chain. Additionally, since same-document navigations don't have
  // initial URLs, ignore empty initial URLs.
  if (!initial_url.is_empty() && final_url != initial_url)
    navigation_data.urls = {initial_url};
  navigation_data.urls.push_back(final_url);

  navigation_data.is_same_document_navigation =
      navigation_handle->IsSameDocument();

  navigation_data.same_origin_status = UkmSource::NavigationData::
      SourceSameOriginStatus::SOURCE_SAME_ORIGIN_STATUS_UNSET;
  // Only set the same origin flag for committed non-error,
  // non-same-document navigations.
  if (navigation_handle->HasCommitted() && !navigation_handle->IsErrorPage() &&
      !navigation_handle->IsSameDocument()) {
    navigation_data.same_origin_status =
        navigation_handle->IsSameOrigin()
            ? UkmSource::NavigationData::SourceSameOriginStatus::
                  SOURCE_SAME_ORIGIN
            : UkmSource::NavigationData::SourceSameOriginStatus::
                  SOURCE_CROSS_ORIGIN;
  }
  navigation_data.is_renderer_initiated =
      navigation_handle->IsRendererInitiated();
  navigation_data.is_error_page = navigation_handle->IsErrorPage();

  navigation_data.previous_source_id =
      last_committed_full_navigation_source_id_;

  navigation_data.navigation_time = navigation_handle->NavigationStart();

  // If the last_committed_full_navigation_or_same_document_source_id_ isn't
  // equal to the last_committed_full_navigation_source_id_, it indicates the
  // previous source was a same document navigation.
  const bool previous_source_was_same_document_navigation =
      last_committed_full_navigation_or_same_document_source_id_ !=
      last_committed_full_navigation_source_id_;
  if (previous_source_was_same_document_navigation) {
    navigation_data.previous_same_document_source_id =
        last_committed_full_navigation_or_same_document_source_id_;
  }
  navigation_data.opener_source_id = opener_source_id_;
  navigation_data.tab_id = tab_id_;

  const ukm::SourceId source_id = ukm::ConvertToSourceId(
      navigation_handle->GetNavigationId(), ukm::SourceIdType::NAVIGATION_ID);
  ukm_recorder->RecordNavigation(source_id, navigation_data);
}

}  // namespace internal

void InitializeSourceUrlRecorderForWebContents(
    content::WebContents* web_contents) {
  internal::SourceUrlRecorderWebContentsObserver::CreateForWebContents(
      web_contents);
}

}  // namespace ukm
