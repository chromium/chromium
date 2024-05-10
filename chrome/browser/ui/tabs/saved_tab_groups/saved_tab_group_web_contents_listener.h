// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_WEB_CONTENTS_LISTENER_H_
#define CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_WEB_CONTENTS_LISTENER_H_

#include "base/token.h"
#include "components/favicon/core/favicon_driver.h"
#include "components/favicon/core/favicon_driver_observer.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/page.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace tab_groups {

class SavedTabGroupModel;

class SavedTabGroupWebContentsListener : public content::WebContentsObserver,
                                         public favicon::FaviconDriverObserver {
 public:
  SavedTabGroupWebContentsListener(content::WebContents* web_contents,
                                   base::Token token,
                                   SavedTabGroupModel* model);
  SavedTabGroupWebContentsListener(content::WebContents* web_contents,
                                   content::NavigationHandle* navigation_handle,
                                   base::Token token,
                                   SavedTabGroupModel* model);
  ~SavedTabGroupWebContentsListener() override;

  // content::WebContentsObserver
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void TitleWasSet(content::NavigationEntry* entry) override;

  // favicon::FaviconDriverObserver
  void OnFaviconUpdated(
      favicon::FaviconDriver* favicon_driver,
      FaviconDriverObserver::NotificationIconType notification_icon_type,
      const GURL& icon_url,
      bool icon_url_changed,
      const gfx::Image& image) override;

  void NavigateToUrl(const GURL& url);

  base::Token token() const { return token_; }
  content::WebContents* web_contents() const { return web_contents_; }

 private:
  const base::Token token_;
  const raw_ptr<content::WebContents> web_contents_;
  // Used to update the favicon for this tab.
  const raw_ptr<favicon::FaviconDriver> favicon_driver_;
  const raw_ptr<SavedTabGroupModel> model_;

  // The NavigationHandle that resulted from the last sync update. Ignored by
  // `DidFinishNavigation` to prevent synclones.
  raw_ptr<content::NavigationHandle> handle_from_sync_update_ = nullptr;
};

}  // namespace tab_groups

#endif  // CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_WEB_CONTENTS_LISTENER_H_
