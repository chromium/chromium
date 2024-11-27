// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_CONTROLLER_H_

#include <map>
#include <memory>
#include <set>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/actions/action_id.h"

namespace page_actions {

class PageActionModel;
class PageActionModelObserver;

// `PageActionController` controls the state of all page actions, scoped to a
// single tab. Each page action has a corresponding `PageActionModel` that will
// receive updates from this controller.
class PageActionController {
 public:
  PageActionController();
  PageActionController(const PageActionController&) = delete;
  PageActionController& operator=(const PageActionController&) = delete;
  ~PageActionController();

  void Initialize(const std::vector<actions::ActionId>& action_ids);
  void Register(actions::ActionId action_id);

  void Show(actions::ActionId action_id);
  void Hide(actions::ActionId action_id);

  // Manages observers for the page action's underlying `PageActionModel`.
  void AddObserver(
      actions::ActionId action_id,
      base::ScopedObservation<PageActionModel, PageActionModelObserver>&
          observation);

 private:
  using PageActionModelsMap =
      std::map<actions::ActionId, std::unique_ptr<PageActionModel>>;

  PageActionModel* FindPageActionModel(actions::ActionId action_id) const;

  PageActionModelsMap page_actions_;
};

}  // namespace page_actions

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_CONTROLLER_H_
