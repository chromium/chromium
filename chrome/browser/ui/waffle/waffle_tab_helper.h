// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WAFFLE_WAFFLE_TAB_HELPER_H_
#define CHROME_BROWSER_UI_WAFFLE_WAFFLE_TAB_HELPER_H_

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}
class Browser;

// Helper class which watches `web_contents` to determine whether there is an
// appropriate opportunity to show the WaffleDialogView.
class WaffleTabHelper : public content::WebContentsObserver,
                        public content::WebContentsUserData<WaffleTabHelper> {
 public:
  WaffleTabHelper(const WaffleTabHelper&) = delete;
  WaffleTabHelper& operator=(const WaffleTabHelper&) = delete;
  ~WaffleTabHelper() override;

 private:
  friend class content::WebContentsUserData<WaffleTabHelper>;

  explicit WaffleTabHelper(content::WebContents* web_contents);

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

// Implemented in `chrome/browser/ui/views/waffle/waffle_dialog_view.cc`
// because there isn't a dependency between `chrome/browser/ui/` and
// `chrome/browser/ui/views/`.
void ShowWaffleDialog(Browser& browser);

#endif  // CHROME_BROWSER_UI_WAFFLE_WAFFLE_TAB_HELPER_H_
