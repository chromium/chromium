// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_TOPICS_BROWSING_TOPICS_REDIRECT_OBSERVER_H_
#define COMPONENTS_BROWSING_TOPICS_BROWSING_TOPICS_REDIRECT_OBSERVER_H_

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace browsing_topics {

// Topics API can be misused when a site gets topics and passes it to the next
// site in the URL param via client-side redirect, and repeating this process
// several times allows the cooperating sites to get all the user's topics upon
// a single page visit.
//
// This class tracks the topics usage in a chain of client-side redirects, by
// getting the previous page's redirect status, updating it, and initializing
// the next page with the updated status.
//
// The redirect chain we are tracking is the sequence of renderer, non-user
// initiated top-level navigations occurring in a single `WebContents`. If a
// navigation ends up in an existing page (in bfcache), then the page's redirect
// status won't be updated, as the page won't be able to learn new information
// (via URL params).
//
// Note that this doesn't perfectly match the misuse pattern:
// - False negative case: It doesn't link a popup page with the opener page.
//   However, as Chrome blocks automated popups by default, we can overlook this
//   exception.
// - False positive case: It may link two pages without direct navigation. For
//   example, the user is on page X, a link is clicked and opens a popup page Y,
//   and Y triggers an automated opener navigation that navigates X to Z. In
//   this case, page X and Z are considered to be in the same redirect chain,
//   although a gesture was involved in this process (from X to Y).
//
// While tracking the navigation initiator's context could help fix these
// issues, it would add complexity. We accept some inaccuracies in favor of a
// simpler approach.
class BrowsingTopicsRedirectObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<BrowsingTopicsRedirectObserver> {
 public:
  static void MaybeCreateForWebContents(content::WebContents* web_contents);

  explicit BrowsingTopicsRedirectObserver(content::WebContents* web_contents);
  BrowsingTopicsRedirectObserver(const BrowsingTopicsRedirectObserver& other) =
      delete;
  BrowsingTopicsRedirectObserver& operator=(
      const BrowsingTopicsRedirectObserver& other) = delete;
  BrowsingTopicsRedirectObserver(BrowsingTopicsRedirectObserver&& other) =
      delete;
  BrowsingTopicsRedirectObserver& operator=(
      BrowsingTopicsRedirectObserver&& other) = delete;
  ~BrowsingTopicsRedirectObserver() override;

 private:
  friend class content::WebContentsUserData<BrowsingTopicsRedirectObserver>;

  // WebContentsObserver:
  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  struct PendingNavigationRedirectState {
    int redirect_count;
    int redirect_with_topics_invoked_count;
    ukm::SourceId source_id_before_redirects;
  };

  // For the ongoing main frame navigation(s), the redirect state to be
  // initialized with for the new page. When a navigation reaches
  // `ReadyToCommitNavigation` and if it's eligible for continued redirect
  // tracking, an entry will be inserted into the map. In `DidFinishNavigation`
  // when the navigation finishes, the entry will be removed from the map.
  //
  // To be on the safe side, we key the state by `NavigationHandle`, so it works
  // even if multiple concurrent navigations reach the state between
  // "ready to commit" and "commit". However, for the navigation type we are
  // tracking (i.e. renderer-initiated, cross-document), we don't believe the
  // race can happen today. Also, when a navigation finishes,
  // `DidFinishNavigation` is guaranteed to be called, so the map won't grow
  // unbounded.
  std::map<raw_ptr<content::NavigationHandle>, PendingNavigationRedirectState>
      pending_navigations_redirect_state_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace browsing_topics

#endif  // COMPONENTS_BROWSING_TOPICS_BROWSING_TOPICS_REDIRECT_OBSERVER_H_
