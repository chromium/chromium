// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_CONTROLLER_H_

#include <map>
#include <memory>
#include <ostream>
#include <set>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/types/pass_key.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/views/page_action/page_action_metrics_recorder_interface.h"
#include "chrome/browser/ui/views/page_action/page_action_model.h"
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

class PageActionView;
class PageActionModelFactory;
class PageActionModelInterface;
class PageActionModelObserver;
class PageActionMetricsRecorderFactory;
class PageActionMetricsRecorderInterface;

// Indicates the source used to color the page action icon.
enum class PageActionColorSource {
  // The foreground's color.
  kForeground,
  // A blend between the focus border color and the background.
  kCascadingAccent,
};

// Configuration for a page action's suggestion chip.
struct SuggestionChipConfig {
  // Whether the chip should have expand/collapse animations.
  // The suggestion chip will only animate once per call to
  // `PageActionController::ShowSuggestionChip`.
  bool should_animate = true;

  // Whether the chip should be announced by a screen reader.
  // TODO(crbug.com/410844651): Consider making this standard behaviour for all
  // page actions.
  bool should_announce_chip = false;

  // Used in tests.
  auto operator<=>(const SuggestionChipConfig& other) const = default;
};

// Represents a scope during which a page action is considered active.
// When this object is destroyed, the activity counter for the associated
// action is decremented.
class ScopedPageActionActivity {
 public:
  ScopedPageActionActivity(PageActionController& controller,
                           actions::ActionId action_id);
  ScopedPageActionActivity(ScopedPageActionActivity&& other) noexcept;
  ScopedPageActionActivity& operator=(
      ScopedPageActionActivity&& other) noexcept;
  ~ScopedPageActionActivity();

  // Not copyable.
  ScopedPageActionActivity(const ScopedPageActionActivity&) = delete;
  ScopedPageActionActivity& operator=(const ScopedPageActionActivity&) = delete;

 private:
  void RegisterWillDestroyControllerCallback();

  raw_ptr<PageActionController> controller_;
  actions::ActionId action_id_;

  base::CallbackListSubscription on_will_destroy_controller_subscription_;
};

std::ostream& operator<<(std::ostream& os, const SuggestionChipConfig& config);

// `PageActionController` controls the state of all page actions, scoped to a
// single tab. Each page action has a corresponding `PageActionModel` that will
// receive updates from this controller.
class PageActionController {
 public:
  virtual ~PageActionController() = default;

  // Requests that the page action be shown or hidden.
  virtual void Show(actions::ActionId action_id) = 0;
  virtual void Hide(actions::ActionId action_id) = 0;

  // Request that the page action's chip state shown or hidden. Note that a
  // request to show the chip does not guarantee it will be shown (for example,
  // the framework may choose to display only one chip at a time, despite
  // requests from multiple features).
  virtual void ShowSuggestionChip(actions::ActionId action_id,
                                  SuggestionChipConfig config) = 0;
  virtual void ShowSuggestionChip(actions::ActionId action_id) = 0;
  virtual void HideSuggestionChip(actions::ActionId action_id) = 0;

  // By default, in suggestion chip mode, the ActionItem text will be used as
  // the control label. However, features can provide a custom text to use
  // as the label. In that case, the custom text will take precedence over
  // the ActionItem text.
  virtual void OverrideText(actions::ActionId action_id,
                            const std::u16string& override_text) = 0;
  virtual void ClearOverrideText(actions::ActionId action_id) = 0;

  // By default, the text is used as the accessible name. However, features may
  // need a different text.
  virtual void OverrideAccessibleName(
      actions::ActionId action_id,
      const std::u16string& override_accessible_name) = 0;
  virtual void ClearOverrideAccessibleName(actions::ActionId action_id) = 0;

  // By default, the page action will have an image which can be shared in the
  // other places that rely on the same action item. However, features can
  // provide a custom image to use for the page action for a specific context
  // (tab). The source of the icon's color can be controlled with
  // `color_source`, which defaults to using foreground color.
  virtual void OverrideImage(actions::ActionId action_id,
                             const ui::ImageModel& override_image) = 0;
  virtual void OverrideImage(actions::ActionId action_id,
                             const ui::ImageModel& override_image,
                             PageActionColorSource color_source) = 0;

  virtual void ClearOverrideImage(actions::ActionId action_id) = 0;

  // By default, the page action will have an tooltip which can be shared in the
  // other places that rely on the same action item. However, features can
  // provide a custom tooltip to use for the page action for a specific context
  // (tab).
  virtual void OverrideTooltip(actions::ActionId action_id,
                               const std::u16string& override_tooltip) = 0;
  virtual void ClearOverrideTooltip(actions::ActionId action_id) = 0;

  // Adds a scope of activity for the given action. Returns a scoped object
  // that manages the activity counter. The action is considered active as
  // long as at least one ScopedPageActionActivity object exists for it.
  virtual ScopedPageActionActivity AddActivity(actions::ActionId action_id) = 0;

  // Adds an observer for the page action's underlying `PageActionModel`.
  virtual void AddObserver(
      actions::ActionId action_id,
      base::ScopedObservation<PageActionModelInterface,
                              PageActionModelObserver>& observation) = 0;

  // Subscribes this controller to updates in the supplied ActionItem, and
  // returns the created subscription. This allows the subscription to be
  // managed by something other than the controller (eg. a view).
  virtual base::CallbackListSubscription CreateActionItemSubscription(
      actions::ActionItem* action_item) = 0;

  // Forces all page actions managed by this controller to be hidden, regardless
  // of whether they would otherwise be visible. Setting it to `false` reverts
  // back to each page action's normal visibility logic.
  virtual void SetShouldHidePageActions(bool should_hide_page_actions) = 0;

  // Registers a callback executed right before the controller is destroyed.
  virtual base::CallbackListSubscription RegisterOnWillDestroyCallback(
      base::OnceCallback<void(PageActionController&)> callback) = 0;

  // Provides a metric recording callback to the caller. The callback won't run
  // if the page action controller is destroyed.
  virtual base::RepeatingCallback<void(PageActionTrigger)> GetClickCallback(
      base::PassKey<PageActionView>,
      actions::ActionId action_id) = 0;

  // Subscribes this controller to get `page_action_view` complete chip
  // visibility change (it final state after animation).
  virtual void RegisterIsChipShowingChangedCallback(
      base::PassKey<PageActionView>,
      actions::ActionId action_id,
      PageActionView* page_action_view) = 0;

  static base::PassKey<PageActionController> PassKeyForTesting() {
    return base::PassKey<PageActionController>();
  }

 protected:
  static base::PassKey<PageActionController> PassKey() {
    return base::PassKey<PageActionController>();
  }

 private:
  friend class ScopedPageActionActivity;

  virtual void DecrementActivityCounter(actions::ActionId action_id) = 0;
};

class PageActionControllerImpl : public PageActionController,
                                 public PinnedToolbarActionsModel::Observer {
 public:
  explicit PageActionControllerImpl(
      PinnedToolbarActionsModel* pinned_actions_model,
      PageActionModelFactory* page_action_model_factory = nullptr,
      PageActionMetricsRecorderFactory* page_action_metrics_factory = nullptr);
  PageActionControllerImpl(const PageActionControllerImpl&) = delete;
  PageActionControllerImpl& operator=(const PageActionControllerImpl&) = delete;
  ~PageActionControllerImpl() override;

  void Initialize(
      tabs::TabInterface& tab_interface,
      const std::vector<actions::ActionId>& action_ids,
      const PageActionPropertiesProviderInterface& properties_provider);

  // PageActionController:
  void Show(actions::ActionId action_id) override;
  void Hide(actions::ActionId action_id) override;
  void ShowSuggestionChip(actions::ActionId action_id) override;
  void ShowSuggestionChip(actions::ActionId action_id,
                          SuggestionChipConfig config) override;
  void HideSuggestionChip(actions::ActionId action_id) override;
  void OverrideText(actions::ActionId action_id,
                    const std::u16string& override_text) override;
  void ClearOverrideText(actions::ActionId action_id) override;
  void OverrideAccessibleName(
      actions::ActionId action_id,
      const std::u16string& override_accessible_name) override;
  void ClearOverrideAccessibleName(actions::ActionId action_id) override;
  void OverrideImage(actions::ActionId action_id,
                     const ui::ImageModel& override_image) override;
  void OverrideImage(actions::ActionId action_id,
                     const ui::ImageModel& override_image,
                     PageActionColorSource color_source) override;
  void ClearOverrideImage(actions::ActionId action_id) override;
  void OverrideTooltip(actions::ActionId action_id,
                       const std::u16string& override_tooltip) override;
  void ClearOverrideTooltip(actions::ActionId action_id) override;
  ScopedPageActionActivity AddActivity(actions::ActionId action_id) override;
  void AddObserver(
      actions::ActionId action_id,
      base::ScopedObservation<PageActionModelInterface,
                              PageActionModelObserver>& observation) override;
  base::CallbackListSubscription CreateActionItemSubscription(
      actions::ActionItem* action_item) override;
  void SetShouldHidePageActions(bool should_hide_page_actions) override;
  base::RepeatingCallback<void(PageActionTrigger)> GetClickCallback(
      base::PassKey<PageActionView>,
      actions::ActionId action_id) override;
  base::CallbackListSubscription RegisterOnWillDestroyCallback(
      base::OnceCallback<void(PageActionController&)> callback) override;
  void RegisterIsChipShowingChangedCallback(
      base::PassKey<PageActionView>,
      actions::ActionId action_id,
      PageActionView* page_action_view) override;

  // PinnedToolbarActionsModel::Observer
  void OnActionsChanged() override;

 private:
  using PageActionModelsMap =
      std::map<actions::ActionId, std::unique_ptr<PageActionModelInterface>>;
  using PageActionMetricsRecordersMap =
      std::map<actions::ActionId,
               std::unique_ptr<PageActionPerActionMetricsRecorderInterface>>;

  // Called by ScopedPageActionActivity when it's destroyed.
  void DecrementActivityCounter(actions::ActionId action_id) override;

  // Creates a page action model for the given id, and initializes it's values.
  void Register(actions::ActionId action_id,
                bool is_tab_active,
                bool is_ephemeral,
                bool is_exempt_from_omnibox_suppression);

  // Triggered when `page_action_view` chip state visibility has changed and
  // completed animation to the new state.
  void OnIsChipShowingChanged(actions::ActionId id, bool is_chip_showing);

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

  // Tracks the number of active scopes for each action.
  std::map<actions::ActionId, int> activity_counters_;

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

  base::OnceCallbackList<void(PageActionController&)>
      on_will_destroy_callback_list_;

  base::WeakPtrFactory<PageActionControllerImpl> weak_factory_{this};
};

}  // namespace page_actions

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_CONTROLLER_H_
