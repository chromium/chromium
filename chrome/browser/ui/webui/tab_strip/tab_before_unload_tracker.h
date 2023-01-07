// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TAB_STRIP_TAB_BEFORE_UNLOAD_TRACKER_H_
#define CHROME_BROWSER_UI_WEBUI_TAB_STRIP_TAB_BEFORE_UNLOAD_TRACKER_H_

#include "content/public/browser/web_contents.h"

namespace tab_strip_ui {

// This class keeps track of WebContents observers that listen for when a tab
// is actually closed or when a user cancels out of a beforeunload confirm
// dialog. The observers are added once a user has swiped on a tab in the
// WebUI tab strip and is needed to make the swiped tab in the tab strip
// visible again if a user cancels out of the close flow.
class TabBeforeUnloadTracker {
 public:
  using TabCloseCancelledCallback =
      base::RepeatingCallback<void(content::WebContents*)>;

  explicit TabBeforeUnloadTracker(TabCloseCancelledCallback cancelled_callback);
  ~TabBeforeUnloadTracker();

  void Observe(content::WebContents* contents);
  void Unobserve(content::WebContents* contents);
  void OnBeforeUnloadDialogCancelled(content::WebContents* contents);

 private:
  class TabObserver;
  std::map<content::WebContents*, std::unique_ptr<TabObserver>> observers_;
  TabCloseCancelledCallback cancelled_callback_;
};

}  // namespace tab_strip_ui

#endif  // CHROME_BROWSER_UI_WEBUI_TAB_STRIP_TAB_BEFORE_UNLOAD_TRACKER_H_
