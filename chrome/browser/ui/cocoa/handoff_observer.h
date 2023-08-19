// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_HANDOFF_OBSERVER_H_
#define CHROME_BROWSER_UI_COCOA_HANDOFF_OBSERVER_H_

#include "base/memory/raw_ptr.h"

#import <Cocoa/Cocoa.h>

#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class Page;
class WebContents;
}

class Browser;

// A protocol that allows ObjC objects to receive delegate callbacks from
// HandoffObserver.
@protocol HandoffObserverDelegate
- (void)handoffContentsChanged:(content::WebContents*)webContents;
@end

// This class observes changes to the "active URL". This is defined as the
// visible URL of the WebContents of the selected tab of the most recently
// focused browser window.
class HandoffObserver : public BrowserListObserver,
                        public TabStripModelObserver,
                        public content::WebContentsObserver {
 public:
  explicit HandoffObserver(NSObject<HandoffObserverDelegate>* delegate);

  HandoffObserver(const HandoffObserver&) = delete;
  HandoffObserver& operator=(const HandoffObserver&) = delete;

  ~HandoffObserver() override;

 private:  // BrowserListObserver
  void OnBrowserSetLastActive(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;

  // TabStripModelObserver
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  // content::WebContentsObserver
  void PrimaryPageChanged(content::Page& page) override;
  void TitleWasSet(content::NavigationEntry* entry) override;

  // Updates the active browser.
  void SetActiveBrowser(Browser* active_browser);

  // Makes this object start observing the WebContents, if it is not already
  // doing so. This method is idempotent.
  void StartObservingWebContents(content::WebContents* web_contents);

  // Makes this object stop observing the WebContents.
  void StopObservingWebContents();

  // Returns the active WebContents. May return nullptr.
  content::WebContents* GetActiveWebContents();

  // This pointer is always up to date, and points to the most recently
  // activated browser, or nullptr if no browsers exist.
  raw_ptr<Browser> active_browser_ = nullptr;

  // Instances of this class should be owned by their |delegate_|.
  NSObject<HandoffObserverDelegate>* __weak delegate_;
};

#endif  // CHROME_BROWSER_UI_COCOA_HANDOFF_OBSERVER_H_
