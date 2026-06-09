// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PAGE_ACTION_PAGE_ACTION_CONTROLLER_H_
#define CHROME_BROWSER_UI_PAGE_ACTION_PAGE_ACTION_CONTROLLER_H_

#include <map>
#include <memory>
#include <optional>
#include <ostream>
#include <set>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "base/types/pass_key.h"
#include "chrome/browser/ui/page_action/page_action_metrics_recorder_interface.h"
#include "chrome/browser/ui/page_action/page_action_pass_key.h"
#include "chrome/browser/ui/page_action/page_action_triggers.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/actions/action_id.h"
#include "ui/base/models/image_model.h"

namespace actions {
class ActionItem;
}

namespace base {
class CallbackListSubscription;
}

namespace ui {
class SimpleMenuModel;
}  // namespace ui

namespace page_actions {

class PageActionModelFactory;
class PageActionModelInterface;
class PageActionModelObserver;
class PageActionMetricsRecorderFactory;
class PageActionMetricsRecorderInterface;
class ChipSelector;
class PageActionController;
class PageActionPropertiesProviderInterface;

// Indicates the source used to color the page action icon.
enum class PageActionColorSource {
  // The foreground's color.
  kForeground,
  // A blend between the focus border color and the background.
  kCascadingAccent,
};

// These values are used for deciding priority when deciding which Anchored
// Message and/or Suggestion Chip should be shown when multiple request to be
// shown.
enum class PageActionPriorityCategory {
  kUnknown = 0,
  kDiscoveryNudge,
  kCoreSiteUtility,
  kContextualCue,
  kPrivacySecurity,
  kUserInteraction,  // This priority is only used for Anchored Messages.
  kMaxValue = kUserInteraction,
};

// Indicates possible anchored message action icons (right side of anchored
// message).
enum class AnchoredMessageActionIconType {
  // No action icon.
  kNone,
  // Close icon.
  kClose,
  // 3-dot menu icon (will be treated as kNone if no actions specified).
  kMenu,
};

// An item within the optional expanded section of an anchored message.
struct AnchoredMessageExpandableItem {
  std::optional<ui::ImageModel> icon;
  std::u16string text;

  bool operator==(const AnchoredMessageExpandableItem&) const = default;
};

// The content specific to the expanded section of an anchored message.
struct AnchoredMessageExpandableContent {
  std::optional<std::u16string> heading;
  std::vector<AnchoredMessageExpandableItem> items;
  // If set, overrides the default tooltip on the expand button.
  std::optional<std::u16string> expand_button_tooltip;
  // If set, overrides the default tooltip on the expand button when the drawer
  // is expanded.
  std::optional<std::u16string> collapse_button_tooltip;

  bool operator==(const AnchoredMessageExpandableContent&) const = default;
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

  // What priority this suggestion chip is.
  PageActionPriorityCategory priority = PageActionPriorityCategory::kUnknown;

  // Used in tests.
  auto operator<=>(const SuggestionChipConfig& other) const = default;
};

// Configuration for a page action's anchored message.
struct AnchoredMessageConfig {
  // What priority this suggestion chip is.
  PageActionPriorityCategory priority = PageActionPriorityCategory::kUnknown;

  auto operator<=>(const AnchoredMessageConfig& other) const = default;
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
  // Interface implemented by the View to allow the Controller to push
  // internal callbacks without a direct dependency on the View class.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    using IsChipShowingChangedCallback =
        base::RepeatingCallback<void(bool is_chip_showing)>;
    using ImageAnimationStartedCallback = base::RepeatingClosure;
    using AnchoredMessageCloseCallback = base::RepeatingClosure;
    using AnchoredMessageExpandCallback = base::RepeatingClosure;
    using AnchoredMessageCollapseCallback = base::RepeatingClosure;
    using ClickCallback = base::RepeatingCallback<void(PageActionTrigger)>;

    virtual void SetIsChipShowingChangedCallback(
        IsChipShowingChangedCallback callback) = 0;
    virtual void SetImageAnimationStartedCallback(
        ImageAnimationStartedCallback callback) = 0;
    virtual void SetAnchoredMessageCloseCallback(
        AnchoredMessageCloseCallback callback) = 0;
    virtual void SetAnchoredMessageExpandCallback(
        AnchoredMessageExpandCallback callback) = 0;
    virtual void SetAnchoredMessageCollapseCallback(
        AnchoredMessageCollapseCallback callback) = 0;
    virtual void SetClickCallback(ClickCallback callback) = 0;
  };

  virtual ~PageActionController() = default;

  // Requests that the page action be shown or hidden.
  virtual void Show(actions::ActionId action_id) = 0;
  virtual void Hide(actions::ActionId action_id) = 0;

  // Request that the page action's chip state shown or hidden. Note that a
  // request to show the chip does not guarantee it will be shown (for example,
  // the framework may choose to display only one chip at a time, despite
  // requests from multiple features).
  virtual void ShowSuggestionChip(actions::ActionId action_id,
                                  const SuggestionChipConfig& config) = 0;
  virtual void ShowSuggestionChip(actions::ActionId action_id) = 0;
  virtual void HideSuggestionChip(actions::ActionId action_id) = 0;

  // Request that the page action's anchored message state shown or hidden. Note
  // that a request to show the anchored message does not guarantee that it will
  // be shown. The framework currently supports a page action showing an
  // anchored message or a suggestion chip, not both. A later successful show
  // request will override an earlier one.
  virtual void ShowAnchoredMessage(actions::ActionId action_id,
                                   const AnchoredMessageConfig& config) = 0;
  virtual void HideAnchoredMessage(actions::ActionId action_id) = 0;

  // Get the ID of the active anchored message, or nullopt if none is showing.
  virtual std::optional<actions::ActionId> GetActiveAnchoredMessage() const = 0;

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
  // `color_source`, which defaults to using foreground color. Optionally, also
  // plays an lottie animation specified by `animation_resource_id`
  // when setting the new override image.
  virtual void OverrideImage(actions::ActionId action_id,
                             const ui::ImageModel& override_image) = 0;
  virtual void OverrideImage(actions::ActionId action_id,
                             const ui::ImageModel& override_image,
                             PageActionColorSource color_source) = 0;
  virtual void OverrideImage(actions::ActionId action_id,
                             const ui::ImageModel& override_image,
                             PageActionColorSource color_source,
                             std::optional<int> animation_resource_id) = 0;

  virtual void ClearOverrideImage(actions::ActionId action_id) = 0;

  // By default, the page action will have an tooltip which can be shared in the
  // other places that rely on the same action item. However, features can
  // provide a custom tooltip to use for the page action for a specific context
  // (tab).
  virtual void OverrideTooltip(actions::ActionId action_id,
                               const std::u16string& override_tooltip) = 0;
  virtual void ClearOverrideTooltip(actions::ActionId action_id) = 0;

  // Functions to set configs for anchored messages.
  virtual void SetAnchoredMessageText(
      actions::ActionId action_id,
      const std::u16string& anchored_message_text) = 0;
  // Sets the anchored message action icon type and menu model. If action icon
  // type is kNone or kClose, the menu model must be null, and if action icon
  // type is kMenu, the model must be non-null.
  virtual void SetAnchoredMessageAction(
      actions::ActionId action_id,
      AnchoredMessageActionIconType action_icon_type,
      std::unique_ptr<ui::SimpleMenuModel> model) = 0;
  virtual void SetAnchoredMessageIcon(actions::ActionId action_id,
                                      const ui::ImageModel& icon) = 0;
  virtual void ClearAnchoredMessageIcon(actions::ActionId action_id) = 0;

  // Sets the expandable content to be displayed in the anchored message bubble.
  // If supplied, the bubble provides a button to toggle showing the content.
  // It is initially hidden. The content specification is intended to be
  // reasonably generic, and may be augmented over time to support the use-cases
  // of various clients.  Initially, it will contain a heading and a list of
  // items containing an image and text.  Supplying `std::nullopt` clears
  // previously-provided content.
  virtual void SetAnchoredMessageExpandableContent(
      actions::ActionId action_id,
      std::optional<AnchoredMessageExpandableContent> expandable_content) = 0;

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

  // Subscribes this controller to get `page_action_view` complete chip
  // visibility change (it final state after animation).
  virtual void RegisterCallbacks(PageActionPassKey pass_key,
                                 actions::ActionId action_id,
                                 Delegate* delegate) = 0;

  static PageActionPassKey PassKeyForTesting() { return PageActionPassKey(); }

 protected:
  static PageActionPassKey PassKey() { return PageActionPassKey(); }

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
                          const SuggestionChipConfig& config) override;
  void HideSuggestionChip(actions::ActionId action_id) override;
  void ShowAnchoredMessage(actions::ActionId action_id,
                           const AnchoredMessageConfig& config) override;
  std::optional<actions::ActionId> GetActiveAnchoredMessage() const override;
  void HideAnchoredMessage(actions::ActionId action_id) override;
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
  void OverrideImage(actions::ActionId action_id,
                     const ui::ImageModel& override_image,
                     PageActionColorSource color_source,
                     std::optional<int> animation_resource_id) override;
  void ClearOverrideImage(actions::ActionId action_id) override;
  void OverrideTooltip(actions::ActionId action_id,
                       const std::u16string& override_tooltip) override;
  void ClearOverrideTooltip(actions::ActionId action_id) override;
  void SetAnchoredMessageText(
      actions::ActionId action_id,
      const std::u16string& anchored_message_text) override;
  void SetAnchoredMessageAction(
      actions::ActionId action_id,
      AnchoredMessageActionIconType action_icon_type,
      std::unique_ptr<ui::SimpleMenuModel> model) override;
  void SetAnchoredMessageIcon(actions::ActionId action_id,
                              const ui::ImageModel& icon) override;
  void ClearAnchoredMessageIcon(actions::ActionId action_id) override;
  void SetAnchoredMessageExpandableContent(
      actions::ActionId action_id,
      std::optional<AnchoredMessageExpandableContent> expandable_content)
      override;
  ScopedPageActionActivity AddActivity(actions::ActionId action_id) override;
  void AddObserver(
      actions::ActionId action_id,
      base::ScopedObservation<PageActionModelInterface,
                              PageActionModelObserver>& observation) override;
  base::CallbackListSubscription CreateActionItemSubscription(
      actions::ActionItem* action_item) override;
  void SetShouldHidePageActions(bool should_hide_page_actions) override;
  base::CallbackListSubscription RegisterOnWillDestroyCallback(
      base::OnceCallback<void(PageActionController&)> callback) override;
  void RegisterCallbacks(PageActionPassKey pass_key,
                         actions::ActionId action_id,
                         Delegate* delegate) override;

  // PinnedToolbarActionsModel::Observer
  void OnActionsChanged() override;

 private:
  using PageActionModelsMap =
      std::map<actions::ActionId, std::unique_ptr<PageActionModelInterface>>;

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

  // Triggered when `page_action_view` plays the image transition animation
  // prior to updating to the new image.
  void OnImageAnimationStarted(actions::ActionId id);

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

  // Helper used to create a metric recorder.
  std::unique_ptr<PageActionMetricsRecorderInterface> CreateMetricsRecorder(
      tabs::TabInterface& tab_interface,
      VisibleEphemeralPageActionsCountCallback
          visible_ephemeral_page_actions_count_callback);

  // Issues internally a metric recording for the provided `action_id`.
  void RecordClickMetric(actions::ActionId action_id,
                         PageActionTrigger trigger_source);

  // Returns the number of page actions currently visual in the actual tab that
  // are ephemeral.
  int GetVisibleEphemeralPageActionsCount() const;

  void DoShowSuggestionChip(actions::ActionId action_id,
                            const SuggestionChipConfig& config);
  void DoHideSuggestionChip(actions::ActionId action_id);
  void DoShowAnchoredMessage(actions::ActionId action_id,
                             const AnchoredMessageConfig& config);
  void DoHideAnchoredMessage(actions::ActionId action_id);
  void DowngradeAnchoredMessage(actions::ActionId action_id);

  void PauseAnchoredMessageTimeout(actions::ActionId action_id);
  void ResumeAnchoredMessageTimeout(actions::ActionId action_id);

  const raw_ptr<PageActionModelFactory> page_action_model_factory_ = nullptr;
  const raw_ptr<PageActionMetricsRecorderFactory>
      page_action_metrics_recorder_factory_ = nullptr;

  PageActionModelsMap page_actions_;

  // Tracks the number of active scopes for each action.
  std::map<actions::ActionId, int> activity_counters_;

  // Metrics recorder.
  // Handles logging UMA metrics for all actions in this tab.
  std::unique_ptr<PageActionMetricsRecorderInterface> metrics_recorder_;

  base::ScopedObservation<PinnedToolbarActionsModel,
                          PinnedToolbarActionsModel::Observer>
      pinned_actions_observation_{this};

  base::CallbackListSubscription tab_activated_callback_subscription_;
  base::CallbackListSubscription tab_deactivated_callback_subscription_;

  base::OnceCallbackList<void(PageActionController&)>
      on_will_destroy_callback_list_;
  std::unique_ptr<ChipSelector> chip_selector_;
  base::RetainingOneShotTimer anchored_message_timeout_;
  int anchored_message_timeout_pause_count_ = 0;
  bool anchored_message_has_timeout_ = false;
  std::optional<actions::ActionId> active_anchored_message_;
  std::map<actions::ActionId, PageActionPriorityCategory> default_priorities_;

  base::WeakPtrFactory<PageActionControllerImpl> weak_factory_{this};
};

}  // namespace page_actions

#endif  // CHROME_BROWSER_UI_PAGE_ACTION_PAGE_ACTION_CONTROLLER_H_
