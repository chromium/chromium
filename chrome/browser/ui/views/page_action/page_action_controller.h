// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_CONTROLLER_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/views/page_action/page_action_metrics_recorder_interface.h"
#include "chrome/browser/ui/views/page_action/page_action_properties_provider.h"
#include "chrome/browser/ui/views/page_action/page_action_triggers.h"
#include "components/tabs/public/tab_interface.h"
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
class PageActionMetricsRecorderFactory;
class PageActionMetricsRecorderInterface;

// Configuration for a page action's suggestion chip.
struct SuggestionChipConfig {
  // Whether the chip should have expand/collapse animations.
  bool should_animate = true;

  // Whether the chip should be announced by a screen reader.
  // TODO(crbug.com/410844651): Consider making this standard behaviour for all
  // page actions.
  bool should_announce_chip = false;

  // Used in tests.
  auto operator<=>(const SuggestionChipConfig& other) const = default;
};

std::ostream& operator<<(std::ostream& os, const SuggestionChipConfig& config);

// `PageActionController` controls the state of all page actions, scoped to a
// single tab. Each page action has a corresponding `PageActionModel` that will
// receive updates from this controller.
class PageActionController : public PinnedToolbarActionsModel::Observer {
 public:
  explicit PageActionController(
      PinnedToolbarActionsModel* pinned_actions_model,
      PageActionModelFactory* page_action_model_factory = nullptr,
      PageActionMetricsRecorderFactory* page_action_metrics_factory = nullptr);
  PageActionController(const PageActionController&) = delete;
  PageActionController& operator=(const PageActionController&) = delete;
  ~PageActionController() override;

  void Initialize(
      tabs::TabInterface& tab_interface,
      const std::vector<actions::ActionId>& action_ids,
      const PageActionPropertiesProviderInterface& properties_provider);

  // Request that the page action be shown or hidden.
  void Show(actions::ActionId action_id);
  void Hide(actions::ActionId action_id);

  // Request that the page action's chip state shown or hidden. Note that a
  // request to show the chip does not guarantee it will be shown (for example,
  // the framework may choose to display only one chip at a time, despite
  // requests from multiple features).
  void ShowSuggestionChip(actions::ActionId action_id,
                          SuggestionChipConfig config = SuggestionChipConfig());
  void HideSuggestionChip(actions::ActionId action_id);

  // By default, in suggestion chip mode, the ActionItem text will be used as
  // the control label. However, features can provide a custom text to use
  // as the label. In that case, the custom text will take precedence over
  // the ActionItem text.
  void OverrideText(actions::ActionId action_id,
                    const std::u16string& override_text);
  void ClearOverrideText(actions::ActionId action_id);

  // By default, the text is used as the accessible name. However, features may
  // need a different text.
  void OverrideAccessibleName(actions::ActionId action_id,
                              const std::u16string& override_accessible_name);
  void ClearOverrideAccessibleName(actions::ActionId action_id);

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

  // Forces all page actions managed by this controller to be hidden, regardless
  // of whether they would otherwise be visible. Setting it to `false` reverts
  // back to each page action's normal visibility logic.
  void SetShouldHidePageActions(bool should_hide_page_actions);

  // PinnedToolbarActionsModel::Observer
  void OnActionsChanged() override;

  static base::PassKey<PageActionController> PassKeyForTesting() {
    return base::PassKey<PageActionController>();
  }

  // Provides a metric recording callback to the caller. The callback won't run
  // if the page action controller is destroyed.
  base::RepeatingCallback<void(PageActionTrigger)> GetClickCallback(
      actions::ActionId action_id);

 private:
  using PageActionModelsMap =
      std::map<actions::ActionId, std::unique_ptr<PageActionModelInterface>>;
  using PageActionMetricsRecordersMap =
      std::map<actions::ActionId,
               std::unique_ptr<PageActionPerActionMetricsRecorderInterface>>;

  // Creates a page action model for the given id, and initializes it's values.
  void Register(actions::ActionId action_id,
                bool is_tab_active,
                bool is_ephemeral);

  PageActionModelInterface& FindPageActionModel(
      actions::ActionId action_id) const;

  void OnTabActivated(tabs::TabInterface* tab);
  void OnTabWillDeactivate(tabs::TabInterface* tab);
  void SetModelsTabActive(bool is_active);

  void ActionItemChanged(const actions::ActionItem* action_item);
  void PinnedActionsModelChanged();

  std::unique_ptr<PageActionModelInterface> CreateModel(
      actions::ActionId action_id,
      bool is_ephemeral);

  // Helper used to create per-action metric recorder.
  std::unique_ptr<PageActionPerActionMetricsRecorderInterface>
  CreatePerActionMetricsRecorder(
      tabs::TabInterface& tab_interface,
      const PageActionProperties& properties,
      PageActionModelInterface& model,
      VisibleEphemeralPageActionsCountCallback
          visible_ephemeral_page_actions_count_callback);

  // Helper used to create a page-level metric recorder.
  std::unique_ptr<PageActionPageMetricsRecorderInterface>
  CreatePageMetricsRecorder(tabs::TabInterface& tab_interface,
                            VisibleEphemeralPageActionsCountCallback
                                visible_ephemeral_page_actions_count_callback);

  // Issues internally a metric recording for the provided `action_id`.
  void RecordClickMetric(actions::ActionId action_id,
                         PageActionTrigger trigger_source);

  // Returns the number of page actions currently visual in the actual tab that
  // are ephemeral.
  int GetVisibleEphemeralPageActionsCount() const;

  const raw_ptr<PageActionModelFactory> page_action_model_factory_ = nullptr;
  const raw_ptr<PageActionMetricsRecorderFactory>
      page_action_metrics_recorder_factory_ = nullptr;

  PageActionModelsMap page_actions_;

  // Metrics recorders associated with ephemeral page actions.
  // Each recorder handles logging UMA metrics for one specific action id.
  PageActionMetricsRecordersMap metrics_recorders_;

  // Page-level metric recorder. It's will recorder global metrics that is not
  // scoped to a single page action.
  std::unique_ptr<PageActionPageMetricsRecorderInterface>
      page_metrics_recorder_;

  base::ScopedObservation<PinnedToolbarActionsModel,
                          PinnedToolbarActionsModel::Observer>
      pinned_actions_observation_{this};

  base::CallbackListSubscription tab_activated_callback_subscription_;
  base::CallbackListSubscription tab_deactivated_callback_subscription_;

  base::WeakPtrFactory<PageActionController> weak_factory_{this};
};

}  // namespace page_actions

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_CONTROLLER_H_
