// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_CONTROLLER_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "ui/actions/action_id.h"

namespace actions {
class ActionItem;
}

namespace base {
class CallbackListSubscription;
}

namespace ui {
class ImageModel;
}

namespace page_actions {

class PageActionModelFactory;
class PageActionModelInterface;
class PageActionModelObserver;

// `PageActionController` controls the state of all page actions, scoped to a
// single tab. Each page action has a corresponding `PageActionModel` that will
// receive updates from this controller.
class PageActionController : public PinnedToolbarActionsModel::Observer {
 public:
  explicit PageActionController(
      PinnedToolbarActionsModel* pinned_actions_model,
      PageActionModelFactory* page_action_model_factory = nullptr);
  PageActionController(const PageActionController&) = delete;
  PageActionController& operator=(const PageActionController&) = delete;
  ~PageActionController() override;

  void Initialize(tabs::TabInterface& tab_interface,
                  const std::vector<actions::ActionId>& action_ids);

  // Request that the page action be shown or hidden.
  void Show(actions::ActionId action_id);
  void Hide(actions::ActionId action_id);

  // Request that the page action's chip state shown or hidden. Note that a
  // request to show the chip does not guarantee it will be shown (for example,
  // the framework may choose to display only one chip at a time, despite
  // requests from multiple features).
  void ShowSuggestionChip(actions::ActionId action_id);
  void HideSuggestionChip(actions::ActionId action_id);

  // By default, in suggestion chip mode, the ActionItem text will be used as
  // the control label. However, features can provide a custom text to use
  // as the label. In that case, the custom text will take precedence over
  // the ActionItem text.
  void OverrideText(actions::ActionId action_id,
                    const std::u16string& override_text);
  void ClearOverrideText(actions::ActionId action_id);

  // By default, the page action will have an image which can be shared in the
  // other places that rely on the same action item. However, features can
  // provide a custom image to use for the page action for a specific context
  // (tab).
  void OverrideImage(actions::ActionId action_id,
                     const ui::ImageModel& override_image);
  void ClearOverrideImage(actions::ActionId action_id);

  // By default, the page action will have an tooltip which can be shared in the
  // other places that rely on the same action item. However, features can
  // provide a custom tooltip to use for the page action for a specific context
  // (tab).
  void OverrideTooltip(actions::ActionId action_id,
                       const std::u16string& override_tooltip);
  void ClearOverrideTooltip(actions::ActionId action_id);

  // Manages observers for the page action's underlying `PageActionModel`.
  void AddObserver(
      actions::ActionId action_id,
      base::ScopedObservation<PageActionModelInterface,
                              PageActionModelObserver>& observation);

  // Subscribes this controller to updates in the supplied ActionItem, and
  // returns the created subscription. This allows the subscription to be
  // managed by something other than the controller (eg. a view).
  base::CallbackListSubscription CreateActionItemSubscription(
      actions::ActionItem* action_item);

  // PinnedToolbarActionsModel::Observer
  void OnActionsChanged() override;

  static base::PassKey<PageActionController> PassKeyForTesting() {
    return base::PassKey<PageActionController>();
  }

 private:
  using PageActionModelsMap =
      std::map<actions::ActionId, std::unique_ptr<PageActionModelInterface>>;

  // Creates a page action model for the given id, and initializes it's values.
  void Register(actions::ActionId action_id, bool is_tab_active);

  PageActionModelInterface& FindPageActionModel(
      actions::ActionId action_id) const;

  void OnTabActivated(tabs::TabInterface* tab);
  void OnTabWillDeactivate(tabs::TabInterface* tab);
  void SetModelsTabActive(bool is_active);

  void ActionItemChanged(const actions::ActionItem* action_item);
  void PinnedActionsModelChanged();

  std::unique_ptr<PageActionModelInterface> CreateModel(
      actions::ActionId action_id);

  const raw_ptr<PageActionModelFactory> page_action_model_factory_ = nullptr;

  PageActionModelsMap page_actions_;

  base::ScopedObservation<PinnedToolbarActionsModel,
                          PinnedToolbarActionsModel::Observer>
      pinned_actions_observation_{this};

  base::CallbackListSubscription tab_activated_callback_subscription_;
  base::CallbackListSubscription tab_deactivated_callback_subscription_;
};

}  // namespace page_actions

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_CONTROLLER_H_
