// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_WEB_CONTENTS_LISTENER_H_
#define CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_WEB_CONTENTS_LISTENER_H_

#include <optional>

#include "base/callback_list.h"
#include "base/types/pass_key.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/types.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace tab_groups {

class LocalTabGroupListener;
class TabGroupSyncService;

// Class that maintains a relationship between the webcontents object of a tab
// and the saved tab group tab that exists for that tab. Listens to navigation
// events on the tab and performs actions to the tab group service, and when
// a sync navigation occurs, updates the local webcontents.
class SavedTabGroupWebContentsListener : public content::WebContentsObserver {
 public:
  SavedTabGroupWebContentsListener(TabGroupSyncService* service,
                                   tabs::TabInterface* local_tab);
  ~SavedTabGroupWebContentsListener() override;

  // Method to be called when a navigation request comes in via sync. Sets the
  // property `navigation_initiated_from_sync` on the navigation so that when
  // DidFinishNavigation is invoked we can correctly identify that the
  // navigation was from sync and prevent it from notifying sync back again.
  void NavigateToUrl(base::PassKey<LocalTabGroupListener>, const GURL& url);

  // For testing only.
  void NavigateToUrlForTest(const GURL& url);

  // Accessors.
  LocalTabID local_tab_id() const;

  content::WebContents* contents() const;

  void OnTabDiscarded(tabs::TabInterface* tab_interface,
                      content::WebContents* old_content,
                      content::WebContents* new_content);

  // content::WebContentsObserver
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidGetUserInteraction(const blink::WebInputEvent& event) override;

  // Retrieves the SavedTabGroup that contains the tab |local_tab_|.
  std::optional<SavedTabGroup> saved_group();

 private:
  void NavigateToUrlInternal(const GURL& url);

  // Clear and then update the |tab_redirect_chain_| for the navigation_handle's
  // entire redirect chain (from GetRedirectChain()). only performed if the nav
  // is a MainFrame navigation.
  void UpdateTabRedirectChain(content::NavigationHandle* navigation_handle);

  // Perform the navigation to the local_tab_ to the specified `url`.
  void PerformNavigation(const GURL& url);

  // Functions that are called when the tab switches activation state.
  void OnTabEnteredForeground(tabs::TabInterface* tab_interface);

  // The service used to query and manage SavedTabGroups.
  const raw_ptr<TabGroupSyncService> service_ = nullptr;

  // The local tab that is being listened to.
  const raw_ptr<tabs::TabInterface> local_tab_ = nullptr;

  // The subscription to the tab discarding callback in the `local_tab_`.
  base::CallbackListSubscription tab_discard_subscription_;

  // Subscription that resumes performing a navigation once the tab is
  // foregrounded if the cached_url_ exists.
  base::CallbackListSubscription tab_foregrounded_subscription_;

  // Stored urls which will be navigated to once the tab is foregrounded.
  std::optional<GURL> cached_url_;
};

}  // namespace tab_groups

#endif  // CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_WEB_CONTENTS_LISTENER_H_
