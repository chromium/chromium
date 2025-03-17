// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_OBSERVER_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_OBSERVER_H_

#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "ui/actions/action_id.h"

namespace page_actions {

class PageActionObserverImpl;

// This is a simple snapshot of a page action's state.
// Each feature has a page action *per-tab*; this represents the page action
// for a single tab.
// Note: this represents the framework-driven state of the page action. E.g.,
// the page action may not be visible due to size-constraints in the view; this
// is not captured.
struct PageActionState {
  actions::ActionId action_id;
  bool showing;
};

// PageActionObserver observes for events on a tab's page action.
class PageActionObserver {
 public:
  explicit PageActionObserver(actions::ActionId action_id);
  virtual ~PageActionObserver();
  PageActionObserver(const PageActionObserver&) = delete;
  PageActionObserver& operator=(const PageActionObserver&) = delete;

  // Invoked when the page action icon becomes visible/hidden.
  virtual void OnPageActionIconShown(const PageActionState& page_action) {}
  virtual void OnPageActionIconHidden(const PageActionState& page_action) {}

  // Begins observation of the page action for the given controller.
  void RegisterAsPageActionObserver(PageActionController& controller);

 private:
  const actions::ActionId action_id_;
  std::unique_ptr<PageActionObserverImpl> observer_impl_;
};

}  // namespace page_actions

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_OBSERVER_H_
