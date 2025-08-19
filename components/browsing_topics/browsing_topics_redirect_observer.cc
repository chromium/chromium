// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_topics/browsing_topics_redirect_observer.h"

#include "base/containers/contains.h"
#include "components/browsing_topics/browsing_topics_page_load_data_tracker.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/common/features.h"

namespace browsing_topics {

// static
void BrowsingTopicsRedirectObserver::MaybeCreateForWebContents(
    content::WebContents* web_contents) {
  if (!base::FeatureList::IsEnabled(network::features::kBrowsingTopics)) {
    return;
  }

  BrowsingTopicsRedirectObserver::CreateForWebContents(web_contents);
}

BrowsingTopicsRedirectObserver::BrowsingTopicsRedirectObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<BrowsingTopicsRedirectObserver>(
          *web_contents) {}

BrowsingTopicsRedirectObserver::~BrowsingTopicsRedirectObserver() = default;

BrowsingTopicsRedirectObserver::PendingNavigationRedirectState::
    PendingNavigationRedirectState(
        std::set<HashedHost>
            pending_navigation_redirect_hosts_with_topics_invoked,
        ukm::SourceId source_id_before_redirects)
    : pending_navigation_redirect_hosts_with_topics_invoked(
          std::move(pending_navigation_redirect_hosts_with_topics_invoked)),
      source_id_before_redirects(source_id_before_redirects) {}

BrowsingTopicsRedirectObserver::PendingNavigationRedirectState::
    ~PendingNavigationRedirectState() = default;

BrowsingTopicsRedirectObserver::PendingNavigationRedirectState::
    PendingNavigationRedirectState(PendingNavigationRedirectState&& other)
    : pending_navigation_redirect_hosts_with_topics_invoked(std::move(
          other.pending_navigation_redirect_hosts_with_topics_invoked)),
      source_id_before_redirects(other.source_id_before_redirects) {}

BrowsingTopicsRedirectObserver::PendingNavigationRedirectState&
BrowsingTopicsRedirectObserver::PendingNavigationRedirectState::operator=(
    PendingNavigationRedirectState&& other) {
  if (this != &other) {
    pending_navigation_redirect_hosts_with_topics_invoked =
        std::move(other.pending_navigation_redirect_hosts_with_topics_invoked);
    source_id_before_redirects = other.source_id_before_redirects;
  }
  return *this;
}

void BrowsingTopicsRedirectObserver::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  CHECK(
      !base::Contains(pending_navigations_redirect_state_, navigation_handle));

  if (!navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }

  if (!navigation_handle->IsRendererInitiated()) {
    return;
  }

  if (navigation_handle->GetNavigationInitiatorActivationAndAdStatus() !=
      blink::mojom::NavigationInitiatorActivationAndAdStatus::
          kDidNotStartWithTransientActivation) {
    return;
  }

  // This is the first commit to the RFH (e.g. new tab page).
  if (navigation_handle->GetWebContents()
          ->GetController()
          .IsInitialNavigation()) {
    return;
  }

  // Renderer-initiated same-doc navigations should only go straight to
  // `DidFinishNavigation`, skipping `ReadyToCommitNavigation`.
  CHECK(!navigation_handle->IsSameDocument());

  auto* previous_page_tracker =
      BrowsingTopicsPageLoadDataTracker::GetOrCreateForPage(
          navigation_handle->GetWebContents()
              ->GetPrimaryMainFrame()
              ->GetPage());

  pending_navigations_redirect_state_.emplace(
      navigation_handle,
      PendingNavigationRedirectState(
          previous_page_tracker->redirect_hosts_with_topics_invoked(),
          previous_page_tracker->source_id_before_redirects()));
}

void BrowsingTopicsRedirectObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  auto it = pending_navigations_redirect_state_.find(navigation_handle);

  // The new page isn't eligible for continued redirect tracking (i.e. the early
  // return scenarios in `ReadyToCommitNavigation`), or the navigation does not
  // commit (e.g. same-doc or download). For those cases, there's no need to
  // explicitly initialize the redirect status.
  if (it == pending_navigations_redirect_state_.end()) {
    return;
  }

  auto extracted = pending_navigations_redirect_state_.extract(it);

  // This could happen if the renderer process crashed between
  // `ReadyToCommitNavigation` and `DidFinishNavigation`.
  if (!navigation_handle->HasCommitted()) {
    return;
  }

  content::Page& page = navigation_handle->GetRenderFrameHost()->GetPage();

  auto* page_tracker = BrowsingTopicsPageLoadDataTracker::GetForPage(page);

  // If the page already exists (in bfcache), we don't bother to update its
  // redirect status, as the page won't be able to learn new information (via
  // URL params).
  if (!page_tracker) {
    BrowsingTopicsPageLoadDataTracker::CreateForPage(
        page,
        std::move(extracted.mapped()
                      .pending_navigation_redirect_hosts_with_topics_invoked),
        extracted.mapped().source_id_before_redirects);
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(BrowsingTopicsRedirectObserver);

}  // namespace browsing_topics
