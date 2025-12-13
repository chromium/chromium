// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_PINNED_TOOLBAR_TAB_SEARCH_TOOLBAR_BUTTON_CONTROLLER_H_
#define CHROME_BROWSER_UI_TOOLBAR_PINNED_TOOLBAR_TAB_SEARCH_TOOLBAR_BUTTON_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/views/tab_search_bubble_host_observer.h"

namespace actions {
class ActionItem;
}  // namespace actions

class BrowserView;
class TabSearchBubbleHost;

class TabSearchToolbarButtonController : public TabSearchBubbleHostObserver {
 public:
  explicit TabSearchToolbarButtonController(BrowserView* browser_view);
  ~TabSearchToolbarButtonController() override;

  TabSearchToolbarButtonController(const TabSearchToolbarButtonController&) =
      delete;
  TabSearchToolbarButtonController& operator=(
      const TabSearchToolbarButtonController&) = delete;

  // TabSearchBubbleHostObserver:
  void OnBubbleInitializing() override;
  void OnBubbleDestroying() override;

  void UpdateBubbleHost(TabSearchBubbleHost* new_tab_search_bubble_host);

 private:
  void MaybeHideActionEphemerallyInToolbar();

  // Gets the TabSearch ActionItem from the hosting browser.
  actions::ActionItem* GetTabSearchActionItem();

  // BrowserView hosting the Tab Search toolbar button, outlives this.
  // TODO(crbug.com/417823694): Pass only the specific BrowserView dependencies
  // into the controller.
  const raw_ptr<BrowserView> browser_view_;

  base::ScopedObservation<TabSearchBubbleHost, TabSearchBubbleHostObserver>
      tab_search_bubble_host_observation_{this};

  base::WeakPtrFactory<TabSearchToolbarButtonController> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_PINNED_TOOLBAR_TAB_SEARCH_TOOLBAR_BUTTON_CONTROLLER_H_
