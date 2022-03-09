// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_HANDOFF_ACTIVE_URL_OBSERVER_H_
#define CHROME_BROWSER_UI_COCOA_HANDOFF_ACTIVE_URL_OBSERVER_H_

#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class Page;
class WebContents;
}

class Browser;
class HandoffActiveURLObserverDelegate;

// This class observes changes to the "active URL". This is defined as the
// visible URL of the WebContents of the selected tab of the most recently
// focused browser window.
class HandoffActiveURLObserver : public BrowserListObserver,
                                 public TabStripModelObserver,
                                 public content::WebContentsObserver {
 public:
  explicit HandoffActiveURLObserver(HandoffActiveURLObserverDelegate* delegate);

  HandoffActiveURLObserver(const HandoffActiveURLObserver&) = delete;
  HandoffActiveURLObserver& operator=(const HandoffActiveURLObserver&) = delete;

  ~HandoffActiveURLObserver() override;

 private:
  // BrowserListObserver
  void OnBrowserSetLastActive(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;

  // TabStripModelObserver
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  // content::WebContentsObserver
  void PrimaryPageChanged(content::Page& page) override;

  // Updates the active browser.
  void SetActiveBrowser(Browser* active_browser);

  // Makes this object start observing the WebContents, if it is not already
  // doing so. This method is idempotent.
  void StartObservingWebContents(content::WebContents* web_contents);

  // Makes this object stop observing the WebContents.
  void StopObservingWebContents();

  // Returns the active WebContents. May return nullptr.
  content::WebContents* GetActiveWebContents();

  // Instances of this class should be owned by their |delegate_|.
  HandoffActiveURLObserverDelegate* delegate_;

  // This pointer is always up to date, and points to the most recently
  // activated browser, or nullptr if no browsers exist.
  Browser* active_browser_;
};

#endif  // CHROME_BROWSER_UI_COCOA_HANDOFF_ACTIVE_URL_OBSERVER_H_
