// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_SCREENTIME_TAB_HELPER_H_
#define CHROME_BROWSER_UI_COCOA_SCREENTIME_TAB_HELPER_H_

#include <memory>

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class Profile;

namespace content {
class WebContents;
}

namespace screentime {

class WebpageController;

// A TabHelper connects a content::WebContents to a WebpageController,
// passing state updates from the WebContents to the WebpageController and
// from the WebpageController to the WebContents or other parts of the browser.
class TabHelper : public content::WebContentsObserver,
                  public content::WebContentsUserData<TabHelper> {
 public:
  static void UseFakeWebpageControllerForTesting();
  static bool IsScreentimeEnabledForProfile(Profile* profile);

  TabHelper(content::WebContents* contents);
  ~TabHelper() override;

  // WebContentsObserver:
  void PrimaryPageChanged(content::Page& handle) override;

  WebpageController* page_controller_for_testing() const {
    return page_controller_.get();
  }

 private:
  friend class content::WebContentsUserData<TabHelper>;

  std::unique_ptr<WebpageController> MakeWebpageController();

  void OnBlockedChanged(bool blocked);

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  std::unique_ptr<WebpageController> page_controller_;
};

}  // namespace screentime

#endif  // CHROME_BROWSER_UI_COCOA_SCREENTIME_TAB_HELPER_H_
