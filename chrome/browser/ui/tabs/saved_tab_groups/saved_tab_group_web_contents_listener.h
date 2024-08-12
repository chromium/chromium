// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_WEB_CONTENTS_LISTENER_H_
#define CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_WEB_CONTENTS_LISTENER_H_

#include <vector>

#include "base/token.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_service_wrapper.h"
#include "components/saved_tab_groups/saved_tab_group.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/page.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace tab_groups {

class SavedTabGroupWebContentsListener : public content::WebContentsObserver {
 public:
  SavedTabGroupWebContentsListener(content::WebContents* web_contents,
                                   base::Token token,
                                   TabGroupServiceWrapper* wrapper_service);
  SavedTabGroupWebContentsListener(content::WebContents* web_contents,
                                   content::NavigationHandle* navigation_handle,
                                   base::Token token,
                                   TabGroupServiceWrapper* wrapper_service);
  ~SavedTabGroupWebContentsListener() override;

  // content::WebContentsObserver
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidGetUserInteraction(const blink::WebInputEvent& event) override;

  void NavigateToUrl(const GURL& url);

  base::Token token() const { return token_; }
  content::WebContents* web_contents() const { return web_contents_; }

 private:
  void UpdateTabRedirectChain(content::NavigationHandle* navigation_handle);

  // Retrieves the SavedTabGroup that contains `token_`.
  const std::optional<SavedTabGroup> saved_group();

  const base::Token token_;
  const raw_ptr<content::WebContents> web_contents_;
  const raw_ptr<TabGroupServiceWrapper> wrapper_service_;

  // Holds the current redirect chain which is used for equality check for any
  // incoming URL update. If any of the URLs in the chain matches with the new
  // URL, we don't do a navigation.
  std::vector<GURL> tab_redirect_chain_;

  // The NavigationHandle that resulted from the last sync update. Ignored by
  // `DidFinishNavigation` to prevent synclones.
  raw_ptr<content::NavigationHandle> handle_from_sync_update_ = nullptr;
};

}  // namespace tab_groups

#endif  // CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_WEB_CONTENTS_LISTENER_H_
