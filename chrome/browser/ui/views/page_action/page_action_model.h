// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_MODEL_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_MODEL_H_

#include <iterator>
#include <memory>
#include <optional>
#include <string>

#include "base/observer_list.h"
#include "base/types/pass_key.h"
#include "ui/base/models/image_model.h"

namespace actions {
class ActionItem;
}  // namespace actions

namespace page_actions {

struct SuggestionChipConfig;
class PageActionController;
class PageActionModelObserver;

// Interface to PageActionModel, used for either the concrete implementation
// or a mock for testing.
class PageActionModelInterface {
 public:
  PageActionModelInterface() = default;
  virtual ~PageActionModelInterface() = default;

  virtual void AddObserver(PageActionModelObserver* observer) = 0;
  virtual void RemoveObserver(PageActionModelObserver* observer) = 0;

  virtual void SetActionItemProperties(
      base::PassKey<PageActionController>,
      const actions::ActionItem* action_item) = 0;
  virtual void SetShowRequested(base::PassKey<PageActionController>,
                                bool requested) = 0;
  virtual void SetShowSuggestionChip(base::PassKey<PageActionController>,
                                     bool show) = 0;
  virtual void SetSuggestionChipConfig(base::PassKey<PageActionController>,
                                       const SuggestionChipConfig& config) = 0;
  virtual void SetTabActive(base::PassKey<PageActionController>,
                            bool is_active) = 0;
  virtual void SetHasPinnedIcon(base::PassKey<PageActionController>,
                                bool has_pinned_icon) = 0;
  virtual void SetOverrideText(
      base::PassKey<PageActionController>,
      const std::optional<std::u16string>& override_text) = 0;
  virtual void SetOverrideAccessibleName(
      base::PassKey<PageActionController>,
      const std::optional<std::u16string>& override_accessible_name) = 0;
  virtual void SetOverrideImage(
      base::PassKey<PageActionController>,
      const std::optional<ui::ImageModel>& override_image) = 0;
  virtual void SetOverrideTooltip(
      base::PassKey<PageActionController>,
      const std::optional<std::u16string>& override_tooltip) = 0;
  virtual void SetShouldHidePageAction(base::PassKey<PageActionController>,
                                       bool should_hide) = 0;

  virtual bool GetVisible() const = 0;
  virtual bool GetShowSuggestionChip() const = 0;
  virtual bool GetShouldAnimateChip() const = 0;
  virtual bool GetShouldAnnounceChip() const = 0;
  virtual const ui::ImageModel& GetImage() const = 0;
  virtual const std::u16string& GetText() const = 0;
  virtual const std::u16string& GetTooltipText() const = 0;
  virtual const std::u16string& GetAccessibleName() const = 0;
  virtual bool GetActionItemIsShowingBubble() const = 0;

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
  void SetActionItemProperties(base::PassKey<PageActionController>,
                               const actions::ActionItem* action_item) override;
  void SetShowRequested(base::PassKey<PageActionController>,
                        bool requested) override;
  void SetShowSuggestionChip(base::PassKey<PageActionController>,
                             bool show) override;
  void SetSuggestionChipConfig(base::PassKey<PageActionController>,
                               const SuggestionChipConfig& config) override;
  void SetTabActive(base::PassKey<PageActionController>,
                    bool is_active) override;
  void SetHasPinnedIcon(base::PassKey<PageActionController>,
                        bool has_pinned_icon) override;

  void SetOverrideText(
      base::PassKey<PageActionController>,
      const std::optional<std::u16string>& override_text) override;

  void SetOverrideAccessibleName(
      base::PassKey<PageActionController>,
      const std::optional<std::u16string>& override_accessible_name) override;

  void SetOverrideImage(
      base::PassKey<PageActionController>,
      const std::optional<ui::ImageModel>& override_image) override;

  void SetOverrideTooltip(
      base::PassKey<PageActionController>,
      const std::optional<std::u16string>& override_tooltip) override;

  void SetShouldHidePageAction(base::PassKey<PageActionController>,
                               bool should_hide) override;

  // The model distills all visibility properties into a single result.
  bool GetVisible() const override;
  bool GetShowSuggestionChip() const override;
  bool GetShouldAnimateChip() const override;
  bool GetShouldAnnounceChip() const override;

  const ui::ImageModel& GetImage() const override;
  const std::u16string& GetText() const override;
  const std::u16string& GetAccessibleName() const override;
  const std::u16string& GetTooltipText() const override;
  bool GetActionItemIsShowingBubble() const override;

  bool IsEphemeral() const override;

 private:
  // Notifies observers of a model change.
  void NotifyChange();

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
  bool show_suggestion_chip_ = false;

  // Represents whether suggestion chips should animate in/out.
  bool should_animate_ = true;

  // Represents whether suggestion chips should be announced by a screen
  // reader.
  bool should_announce_chip_ = false;

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

  // When set, it will always take precedence over `text_`.
  std::optional<std::u16string> override_text_;

  // When set, it will always take precedence over `text_` because by default
  // `text_` will be used.
  std::optional<std::u16string> override_accessible_name_;

  // Tracks whether we should forcibly hide the page action (e.g., Omnibox is
  // getting updated).
  bool should_hide_ = false;

  // Flag used to disallow reentrant behaviour.
  bool is_notifying_observers_ = false;

  base::ObserverList<PageActionModelObserver> observer_list_;
};

class PageActionModelFactory {
 public:
  virtual ~PageActionModelFactory() = default;

  virtual std::unique_ptr<PageActionModelInterface> Create(
      int action_id,
      bool is_ephemeral) = 0;
};

}  // namespace page_actions

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_MODEL_H_
