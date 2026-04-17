// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_MODEL_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_MODEL_H_

#include <iterator>
#include <memory>
#include <optional>
#include <string>

#include "base/containers/enum_set.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/types/pass_key.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_enums.h"
#include "ui/base/models/image_model.h"

namespace ui {
class SimpleMenuModel;
}

namespace actions {
class ActionItem;
}  // namespace actions

namespace page_actions {

struct SuggestionChipConfig;
class PageActionModelObserver;
enum class PageActionColorSource;
enum class AnchoredMessageActionIconType;

// Interface to PageActionModel, used for either the concrete implementation
// or a mock for testing.
class PageActionModelInterface {
 public:
  PageActionModelInterface() = default;
  virtual ~PageActionModelInterface() = default;

  virtual void AddObserver(PageActionModelObserver* observer) = 0;
  virtual void RemoveObserver(PageActionModelObserver* observer) = 0;

  virtual void SetActionItemProperties(
      PageActionPassKey pass_key,
      const actions::ActionItem* action_item) = 0;
  virtual void SetShowRequested(PageActionPassKey pass_key, bool requested) = 0;
  virtual void SetShouldShowSuggestionChip(PageActionPassKey pass_key,
                                           bool show) = 0;
  virtual void SetSuggestionChipConfig(PageActionPassKey pass_key,
                                       const SuggestionChipConfig& config) = 0;
  virtual void SetShouldShowAnchoredMessage(PageActionPassKey pass_key,
                                            bool show) = 0;
  virtual void SetTabActive(PageActionPassKey pass_key, bool is_active) = 0;
  virtual void SetHasPinnedIcon(PageActionPassKey pass_key,
                                bool has_pinned_icon) = 0;
  // TODO(crbug.com/376285838): Move overrides to SuggestionChip and
  // AnchoredMessage configs.
  virtual void SetOverrideText(
      PageActionPassKey pass_key,
      const std::optional<std::u16string>& override_text) = 0;
  virtual void SetOverrideAccessibleName(
      PageActionPassKey pass_key,
      const std::optional<std::u16string>& override_accessible_name) = 0;
  virtual void SetOverrideImage(
      PageActionPassKey pass_key,
      const std::optional<ui::ImageModel>& override_image,
      PageActionColorSource color_source) = 0;
  virtual void SetOverrideTooltip(
      PageActionPassKey pass_key,
      const std::optional<std::u16string>& override_tooltip) = 0;
  virtual void SetAnchoredMessageText(
      PageActionPassKey pass_key,
      const std::u16string& anchored_message) = 0;
  virtual void SetAnchoredMessageAction(
      PageActionPassKey pass_key,
      const AnchoredMessageActionIconType action_icon_type,
      std::unique_ptr<ui::SimpleMenuModel> model) = 0;
  virtual void SetAnchoredMessageIcon(
      PageActionPassKey pass_key,
      const std::optional<ui::ImageModel>& icon) = 0;
  virtual void SetActionActive(PageActionPassKey pass_key, bool is_active) = 0;
  virtual void SetIsSuppressedByOmnibox(PageActionPassKey pass_key,
                                        bool is_suppressed) = 0;
  virtual void SetExemptFromOmniboxSuppression(PageActionPassKey pass_key,
                                               bool is_exempt) = 0;
  virtual void SetIsChipShowing(PageActionPassKey pass_key,
                                bool is_chip_showing) = 0;
  virtual void SetIsAnchoredMessageShowing(
      PageActionPassKey pass_key,
      bool is_anchored_message_showing) = 0;

  virtual bool GetVisible() const = 0;
  virtual bool IsChipShowing() const = 0;
  virtual bool ShouldShowSuggestionChip() const = 0;
  virtual bool GetShouldAnimateChipOut() const = 0;
  virtual bool GetShouldAnimateChipIn() const = 0;
  virtual bool GetShouldAnnounceChip() const = 0;
  virtual bool ShouldShowAnchoredMessage() const = 0;
  virtual bool IsAnchoredMessageShowing() const = 0;
  virtual const ui::ImageModel& GetImage() const = 0;
  virtual const std::u16string& GetText() const = 0;
  virtual const std::u16string& GetTooltipText() const = 0;
  virtual const std::u16string& GetAccessibleName() const = 0;
  virtual const std::u16string& GetAnchoredMessageText() const = 0;
  virtual const std::optional<ui::ImageModel>& GetAnchoredMessageIcon()
      const = 0;
  virtual AnchoredMessageActionIconType GetAnchoredMessageActionIconType()
      const = 0;
  virtual ui::SimpleMenuModel* GetAnchoredMessageMenuModel() const = 0;
  virtual bool GetActionItemIsShowingBubble() const = 0;
  virtual bool GetActionActive() const = 0;
  virtual PageActionColorSource GetColorSource() const = 0;

  virtual bool IsEphemeral() const = 0;
};

// PageActionModel represents the page action's state, scoped to a single tab.
class PageActionModel : public PageActionModelInterface {
 public:
  explicit PageActionModel(bool is_ephemeral = false);
  PageActionModel(const PageActionModel&) = delete;
  PageActionModel& operator=(const PageActionModel&) = delete;
  ~PageActionModel() override;

  void AddObserver(PageActionModelObserver* observer) override;
  void RemoveObserver(PageActionModelObserver* observer) override;

  // Applies any relevant ActionItem properties to the model, including
  // visibility, text and icon properties.
  void SetActionItemProperties(PageActionPassKey pass_key,
                               const actions::ActionItem* action_item) override;
  void SetShowRequested(PageActionPassKey pass_key, bool requested) override;
  void SetShouldShowSuggestionChip(PageActionPassKey pass_key,
                                   bool show) override;
  void SetSuggestionChipConfig(PageActionPassKey pass_key,
                               const SuggestionChipConfig& config) override;
  void SetShouldShowAnchoredMessage(PageActionPassKey pass_key,
                                    bool show) override;
  void SetTabActive(PageActionPassKey pass_key, bool is_active) override;
  void SetHasPinnedIcon(PageActionPassKey pass_key,
                        bool has_pinned_icon) override;

  void SetOverrideText(
      PageActionPassKey pass_key,
      const std::optional<std::u16string>& override_text) override;

  void SetOverrideAccessibleName(
      PageActionPassKey pass_key,
      const std::optional<std::u16string>& override_accessible_name) override;

  void SetOverrideImage(PageActionPassKey pass_key,
                        const std::optional<ui::ImageModel>& override_image,
                        PageActionColorSource color_source) override;

  void SetOverrideTooltip(
      PageActionPassKey pass_key,
      const std::optional<std::u16string>& override_tooltip) override;

  void SetAnchoredMessageText(PageActionPassKey pass_key,
                              const std::u16string& anchored_message) override;

  void SetAnchoredMessageAction(
      PageActionPassKey pass_key,
      const AnchoredMessageActionIconType action_icon_type,
      std::unique_ptr<ui::SimpleMenuModel> model) override;

  void SetAnchoredMessageIcon(
      PageActionPassKey pass_key,
      const std::optional<ui::ImageModel>& icon) override;

  void SetActionActive(PageActionPassKey pass_key, bool is_active) override;

  void SetIsSuppressedByOmnibox(PageActionPassKey pass_key,
                                bool is_suppressed) override;

  void SetExemptFromOmniboxSuppression(PageActionPassKey pass_key,
                                       bool is_exempt) override;

  void SetIsChipShowing(PageActionPassKey pass_key,
                        bool is_chip_showing) override;

  void SetIsAnchoredMessageShowing(PageActionPassKey pass_key,
                                   bool is_anchored_message_showing) override;

  // The model distills all visibility properties into a single result.
  bool GetVisible() const override;
  bool IsChipShowing() const override;
  bool ShouldShowSuggestionChip() const override;
  bool GetShouldAnimateChipOut() const override;
  bool GetShouldAnimateChipIn() const override;
  bool GetShouldAnnounceChip() const override;
  bool ShouldShowAnchoredMessage() const override;
  bool IsAnchoredMessageShowing() const override;

  const ui::ImageModel& GetImage() const override;
  const std::u16string& GetText() const override;
  const std::u16string& GetAccessibleName() const override;
  const std::u16string& GetAnchoredMessageText() const override;
  AnchoredMessageActionIconType GetAnchoredMessageActionIconType()
      const override;
  ui::SimpleMenuModel* GetAnchoredMessageMenuModel() const override;
  const std::optional<ui::ImageModel>& GetAnchoredMessageIcon() const override;
  const std::u16string& GetTooltipText() const override;
  bool GetActionItemIsShowingBubble() const override;
  bool GetActionActive() const override;
  PageActionColorSource GetColorSource() const override;

  bool IsEphemeral() const override;

 private:
  // Identifies which property triggered a NotifyChange call, used for
  // per-property reentrancy checks.
  enum class Property {
    kShowRequested,
    kShouldShowSuggestionChip,
    kSuggestionChipConfig,
    kTabActive,
    kHasPinnedIcon,
    kActionItemProperties,
    kOverrideText,
    kOverrideAccessibleName,
    kOverrideImage,
    kOverrideTooltip,
    kIsSuppressedByOmnibox,
    kExemptFromOmniboxSuppression,
    kIsChipShowing,
    kActionActive,
    kShouldShowAnchoredMessage,
    kAnchoredMessageText,
    kAnchoredMessageActionIcon,
    kIsAnchoredMessageShowing,
    kAnchoredMessageIcon,
    kMaxValue = kAnchoredMessageIcon,
  };
  using PropertySet =
      base::EnumSet<Property, Property::kShowRequested, Property::kMaxValue>;

  // Notifies observers of a model change. `property` identifies the property
  // that was modified, used for reentrancy checks. Re-entrant modifications to
  // the same property CHECK-fail, as they would cause an infinite notification
  // loop.
  void NotifyChange(Property property);

  // Represents whether this page action will be always visible or not.
  const bool is_ephemeral_ = false;

  // Represents whether the tab this model belongs to is active.
  bool is_tab_active_ = false;

  // Represents whether this page action has a corresponding pinned icon sharing
  // the same action.
  bool has_pinned_icon_ = false;

  // Represents whether a feature requested to show this page action.
  bool show_requested_ = false;

  // Represents whether the page action associated with this model should show
  // as suggestion chip.
  bool should_show_suggestion_chip_ = false;

  // Represents whether suggestion chips should animate in/out.
  bool should_animate_ = true;

  // Represents whether the suggestion chip is fully expanded or not (in/out
  // animation is completed). Therefore, it should not be animating.
  bool is_chip_showing_ = false;

  // Whether the chip was shown for a given `SetShouldShowSuggestion` request.
  bool did_show_chip_ = false;

  // Represents whether suggestion chips should be announced by a screen
  // reader.
  bool should_announce_chip_ = false;

  // Whether the anchored message is showing.
  bool is_anchored_message_showing_ = false;

  // Wgether the anchored message should be shown.
  bool should_show_anchored_message_ = false;

  // Properties taken from ActionItem.
  bool action_item_enabled_ = false;
  bool action_item_visible_ = false;
  bool action_item_is_showing_bubble_ = false;
  std::u16string text_;
  std::u16string tooltip_;
  // When set, it will always take precedence over `tooltip_`.
  std::optional<std::u16string> override_tooltip_;
  ui::ImageModel action_item_image_;
  // When set, it will always take precedence over `action_item_image_`.
  std::optional<ui::ImageModel> override_image_;
  std::optional<PageActionColorSource> color_source_;

  // When set, it will always take precedence over `text_`.
  std::optional<std::u16string> override_text_;

  // The text to be shown on anchored messages.
  std::u16string anchored_message_text_;
  // Special anchored message icon. If set, the normal page action icon will not
  // show on the anchored message.
  std::optional<ui::ImageModel> anchored_message_icon_ = std::nullopt;

  // When set, it will always take precedence over `text_` because by default
  // `text_` will be used.
  std::optional<std::u16string> override_accessible_name_;

  // Represents whether the action is currently active (e.g. showing dialog).
  bool action_active_ = false;

  // Indicates that the omnibox wants the page action hidden (e.g., Omnibox is
  // getting updated, or omnibox popup is open).
  bool is_suppressed_by_omnibox_ = false;

  // Represents whether this page action should ignore visibility override set
  // by `is_suppressed_by_omnibox_` variable (eg. AI mode page action).
  bool is_exempt_from_omnibox_suppression_ = false;

  // Flag used while notifying observers.
  bool is_notifying_observers_ = false;

  std::unique_ptr<ui::SimpleMenuModel> anchored_message_menu_model_;
  AnchoredMessageActionIconType anchored_message_action_icon_type_ =
      AnchoredMessageActionIconType::kNone;

  // Tracks which properties have been modified during the current notification
  // cycle. Used to detect infinite loops: if the same property is modified
  // again during notification, we CHECK-fail.
  PropertySet notified_properties_;

  base::ObserverList<PageActionModelObserver> observer_list_;
};

class PageActionModelFactory {
 public:
  virtual ~PageActionModelFactory() = default;

  virtual std::unique_ptr<PageActionModelInterface> Create(
      actions::ActionId action_id,
      bool is_ephemeral) = 0;
};

}  // namespace page_actions

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_MODEL_H_
