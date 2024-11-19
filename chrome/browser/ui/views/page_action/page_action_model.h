// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_MODEL_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_MODEL_H_

#include "base/observer_list.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"

namespace page_actions {

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

  // TODO(crbug.com/376285151): Add base::PassKey protection to limit access to
  // the controller.
  void SetVisible(bool visible);

 private:
  bool is_visible_ = false;

  base::ObserverList<PageActionModelObserver> observer_list_;
};

}  // namespace page_actions

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_MODEL_H_
