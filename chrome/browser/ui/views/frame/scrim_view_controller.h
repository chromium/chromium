// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_SCRIM_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_SCRIM_VIEW_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"

class BrowserView;

// Controller that manages the visibility of scrims attached to the
// ContentsContainerViews.
class ScrimViewController : public TabStripModelObserver {
 public:
  explicit ScrimViewController(BrowserView* browser_view);
  ~ScrimViewController() override;

 private:
  // TabStripModelObserver implementation.
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;
  void TabBlockedStateChanged(content::WebContents* contents,
                              int index) override;
  void OnSplitTabChanged(const SplitTabChange& change) override;

  void UpdateScrimViews();

  raw_ptr<BrowserView> browser_view_ = nullptr;
  raw_ptr<TabStripModel> tab_strip_model_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_SCRIM_VIEW_CONTROLLER_H_
