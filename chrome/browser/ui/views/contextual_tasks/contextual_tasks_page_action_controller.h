// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_PAGE_ACTION_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_PAGE_ACTION_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace tabs {
class TabInterface;
}  // namespace tabs

// Controller used to trigger the contextual task page action chip to show/hide.
class ContextualTasksPageActionController {
 public:
  DECLARE_USER_DATA(ContextualTasksPageActionController);
  explicit ContextualTasksPageActionController(
      tabs::TabInterface* tab_interface);
  ~ContextualTasksPageActionController();

  static ContextualTasksPageActionController* From(
      tabs::TabInterface* tab_interface);

  void InvokePageAction();

 private:
  raw_ptr<tabs::TabInterface> tab_interface_ = nullptr;

  ui::ScopedUnownedUserData<ContextualTasksPageActionController>
      scoped_unowned_user_data_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_PAGE_ACTION_CONTROLLER_H_
