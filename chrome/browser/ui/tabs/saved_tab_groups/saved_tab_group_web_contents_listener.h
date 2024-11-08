// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_WEB_CONTENTS_LISTENER_H_
#define CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_WEB_CONTENTS_LISTENER_H_

#include "base/callback_list.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/types.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/page.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace tab_groups {

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
  void NavigateToUrl(const GURL& url);

  // Accessors.
  LocalTabID local_tab_id() const;

  content::WebContents* contents() const;

  void OnTabDiscarded(tabs::TabInterface* interface,
                      content::WebContents* old_content,
                      content::WebContents* new_content);

  // content::WebContentsObserver
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidGetUserInteraction(const blink::WebInputEvent& event) override;

 private:
  // Clear and then update the |tab_redirect_chain_| for the navigation_handle's
  // entire redirect chain (from GetRedirectChain()). only performed if the nav
  // is a MainFrame navigation.
  void UpdateTabRedirectChain(content::NavigationHandle* navigation_handle);

  // Retrieves the SavedTabGroup that contains the tab |local_tab_|.
  const SavedTabGroup saved_group();

  // The service used to query and manage SavedTabGroups.
  const raw_ptr<TabGroupSyncService> service_ = nullptr;

  // The local tab that is being listened to.
  const raw_ptr<tabs::TabInterface> local_tab_ = nullptr;

  // The subscription to the tab discarding callback in the `local_tab_`.
  base::CallbackListSubscription tab_discard_subscription_;
};

}  // namespace tab_groups

#endif  // CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_WEB_CONTENTS_LISTENER_H_
