// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_PINNED_TOOLBAR_TAB_SEARCH_TOOLBAR_BUTTON_CONTROLLER_H_
#define CHROME_BROWSER_UI_TOOLBAR_PINNED_TOOLBAR_TAB_SEARCH_TOOLBAR_BUTTON_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/tab_search_bubble_host.h"

class BrowserView;

class TabSearchToolbarButtonController : public TabSearchBubbleHost::Observer {
 public:
  explicit TabSearchToolbarButtonController(BrowserView* browser_view);
  ~TabSearchToolbarButtonController() override;

  TabSearchToolbarButtonController(const TabSearchToolbarButtonController&) =
      delete;
  TabSearchToolbarButtonController& operator=(
      const TabSearchToolbarButtonController&) = delete;

  // TabSearchBubbleHost::Observer:
  void OnBubbleInitializing() override;
  void OnBubbleDestroying() override;

  void UpdateForWebUITabStrip();

 private:
  void MaybeHideActionEphemerallyInToolbar();

  raw_ptr<BrowserView> browser_view_;
  bool bubble_showing_ = false;
  base::WeakPtrFactory<TabSearchToolbarButtonController> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_PINNED_TOOLBAR_TAB_SEARCH_TOOLBAR_BUTTON_CONTROLLER_H_
