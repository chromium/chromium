// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CLOSE_BUBBLE_ON_TAB_ACTIVATION_HELPER_H_
#define CHROME_BROWSER_UI_VIEWS_CLOSE_BUBBLE_ON_TAB_ACTIVATION_HELPER_H_

#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"

class Browser;

namespace views {
class BubbleDialogDelegateView;
}

// Helper class that closes the bubble view every time the active tab changes.
// That is required as on macOS, the user may use the keyboard shortcuts to
// add, close or change the active tab.
class CloseBubbleOnTabActivationHelper : public TabStripModelObserver {
 public:
  // It is the expectation of this class that |bubble| and |browser| should
  // outlive it. The recommended usage is for |bubble| to own |this|.
  CloseBubbleOnTabActivationHelper(
      views::BubbleDialogDelegateView* owner_bubble,
      Browser* browser);
  ~CloseBubbleOnTabActivationHelper() override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

 private:
  views::BubbleDialogDelegateView* owner_bubble_;  // weak, owns me.

  DISALLOW_COPY_AND_ASSIGN(CloseBubbleOnTabActivationHelper);
};

#endif  // CHROME_BROWSER_UI_VIEWS_CLOSE_BUBBLE_ON_TAB_ACTIVATION_HELPER_H_
