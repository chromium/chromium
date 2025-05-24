// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/page_action_model.h"

#include "base/types/pass_key.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_model_observer.h"
#include "ui/actions/actions.h"

namespace page_actions {

namespace {
using ::actions::ActionItem;
}  // namespace

PageActionModel::PageActionModel(bool is_ephemeral)
    : is_ephemeral_(is_ephemeral) {}

PageActionModel::~PageActionModel() {
  observer_list_.Notify(
      &PageActionModelObserver::OnPageActionModelWillBeDeleted, *this);
}

void PageActionModel::SetShowRequested(base::PassKey<PageActionController>,
                                       bool requested) {
  if (show_requested_ == requested) {
    return;
  }
  show_requested_ = requested;
  NotifyChange();
}

void PageActionModel::SetShowSuggestionChip(base::PassKey<PageActionController>,
                                            bool show) {
  if (show_suggestion_chip_ == show) {
    return;
  }
  show_suggestion_chip_ = show;
  NotifyChange();
}

void PageActionModel::SetSuggestionChipConfig(
    base::PassKey<PageActionController>,
    const SuggestionChipConfig& config) {
  if (should_animate_ == config.should_animate &&
      should_announce_chip_ == config.should_announce_chip) {
    return;
  }
  should_animate_ = config.should_animate;
  should_announce_chip_ = config.should_announce_chip;
  NotifyChange();
}

void PageActionModel::SetTabActive(base::PassKey<PageActionController>,
                                   bool is_active) {
  if (is_tab_active_ == is_active) {
    return;
  }
  is_tab_active_ = is_active;
  NotifyChange();
}

void PageActionModel::SetHasPinnedIcon(base::PassKey<PageActionController>,
                                       bool has_pinned_icon) {
  if (has_pinned_icon_ == has_pinned_icon) {
    return;
  }
  has_pinned_icon_ = has_pinned_icon;
  NotifyChange();
}

void PageActionModel::SetActionItemProperties(
    base::PassKey<PageActionController>,
    const ActionItem* action_item) {
  bool model_changed = false;

  if (action_item_enabled_ != action_item->GetEnabled()) {
    action_item_enabled_ = action_item->GetEnabled();
    model_changed = true;
  }
  if (action_item_visible_ != action_item->GetVisible()) {
    action_item_visible_ = action_item->GetVisible();
    model_changed = true;
  }
  if (action_item_image_ != action_item->GetImage()) {
    action_item_image_ = action_item->GetImage();
    model_changed = true;
  }
  if (action_item_is_showing_bubble_ != action_item->GetIsShowingBubble()) {
    action_item_is_showing_bubble_ = action_item->GetIsShowingBubble();
    model_changed = true;
  }
  if (text_ != action_item->GetText()) {
    text_ = action_item->GetText();
    model_changed = true;
  }
  if (tooltip_ != action_item->GetTooltipText()) {
    tooltip_ = action_item->GetTooltipText();
    model_changed = true;
  }

  if (model_changed) {
    NotifyChange();
  }
}

bool PageActionModel::GetVisible() const {
  if (should_hide_) {
    return false;
  }

  return is_tab_active_ && action_item_enabled_ && action_item_visible_ &&
         show_requested_ && !has_pinned_icon_;
}

bool PageActionModel::GetShowSuggestionChip() const {
  return show_suggestion_chip_;
}

bool PageActionModel::GetShouldAnimateChip() const {
  return should_animate_;
}

bool PageActionModel::GetShouldAnnounceChip() const {
  return should_announce_chip_;
}

const ui::ImageModel& PageActionModel::GetImage() const {
  return override_image_.has_value() ? override_image_.value()
                                     : action_item_image_;
}

const std::u16string& PageActionModel::GetText() const {
  return override_text_.has_value() ? override_text_.value() : text_;
}

const std::u16string& PageActionModel::GetAccessibleName() const {
  return override_accessible_name_.has_value()
             ? override_accessible_name_.value()
             : text_;
}

const std::u16string& PageActionModel::GetTooltipText() const {
  return override_tooltip_.has_value() ? override_tooltip_.value() : tooltip_;
}

bool PageActionModel::GetActionItemIsShowingBubble() const {
  return action_item_is_showing_bubble_;
}

void PageActionModel::SetOverrideText(
    base::PassKey<PageActionController>,
    const std::optional<std::u16string>& override_text) {
  if (override_text_ == override_text) {
    return;
  }
  override_text_ = override_text;
  NotifyChange();
}

void PageActionModel::SetOverrideAccessibleName(
    base::PassKey<PageActionController>,
    const std::optional<std::u16string>& override_accessible_name) {
  if (override_accessible_name_ == override_accessible_name) {
    return;
  }
  override_accessible_name_ = override_accessible_name;
  NotifyChange();
}

void PageActionModel::SetOverrideImage(
    base::PassKey<PageActionController>,
    const std::optional<ui::ImageModel>& override_image) {
  if (override_image_ == override_image) {
    return;
  }
  override_image_ = override_image;
  NotifyChange();
}

void PageActionModel::SetOverrideTooltip(
    base::PassKey<PageActionController>,
    const std::optional<std::u16string>& override_tooltip) {
  if (override_tooltip_ == override_tooltip) {
    return;
  }
  override_tooltip_ = override_tooltip;
  NotifyChange();
}

void PageActionModel::SetShouldHidePageAction(
    base::PassKey<PageActionController>,
    bool should_hide) {
  if (should_hide_ == should_hide) {
    return;
  }

  should_hide_ = should_hide;
  NotifyChange();
}

void PageActionModel::AddObserver(PageActionModelObserver* observer) {
  observer_list_.AddObserver(observer);
}

void PageActionModel::RemoveObserver(PageActionModelObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void PageActionModel::NotifyChange() {
  CHECK(!is_notifying_observers_)
      << "PageActionModel should not be updated while notifying observers";
  base::AutoReset<bool> auto_reset(&is_notifying_observers_, true);
  observer_list_.Notify(&PageActionModelObserver::OnPageActionModelChanged,
                        *this);
}

bool PageActionModel::IsEphemeral() const {
  return is_ephemeral_;
}

}  // namespace page_actions
