// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_MODEL_OBSERVER_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_MODEL_OBSERVER_H_

#include "base/observer_list_types.h"

namespace page_actions {

class PageActionModelInterface;

// PageActionModelObserver observes events on a PageActionModel.
class PageActionModelObserver : public base::CheckedObserver {
 public:
  // Invoked when the model's state changes.
  virtual void OnPageActionModelChanged(const PageActionModelInterface& model) {
  }
  // Invoked when the model is being deleted.
  virtual void OnPageActionModelWillBeDeleted(
      const PageActionModelInterface& model) {}
};

}  // namespace page_actions

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_MODEL_OBSERVER_H_
