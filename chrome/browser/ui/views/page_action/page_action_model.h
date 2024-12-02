// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_MODEL_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_MODEL_H_

#include "base/observer_list.h"
#include "base/types/pass_key.h"

namespace page_actions {

class PageActionController;
class PageActionModelObserver;

// PageActionModel represents the page action's state, scoped to a single tab.
class PageActionModel {
 public:
  PageActionModel();
  PageActionModel(const PageActionModel&) = delete;
  PageActionModel& operator=(const PageActionModel&) = delete;
  ~PageActionModel();

  void AddObserver(PageActionModelObserver* observer);
  void RemoveObserver(PageActionModelObserver* observer);

  void SetShowRequested(base::PassKey<PageActionController>, bool requested);

  bool show_requested() const { return show_requested_; }

 private:
  // Represents whether a feature requested to show this page action.
  bool show_requested_ = false;

  base::ObserverList<PageActionModelObserver> observer_list_;
};

}  // namespace page_actions

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_MODEL_H_
