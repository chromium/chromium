// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/page_action_model.h"

#include "base/types/pass_key.h"
#include "chrome/browser/ui/views/page_action/page_action_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_model_observer.h"
#include "ui/actions/actions.h"
#include "ui/menus/simple_menu_model.h"

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

void PageActionModel::SetShowRequested(PageActionPassKey, bool requested) {
  if (show_requested_ == requested) {
    return;
  }
  show_requested_ = requested;
  NotifyChange(Property::kShowRequested);
}

void PageActionModel::SetShouldShowSuggestionChip(PageActionPassKey,
                                                  bool show) {
  did_show_chip_ = false;
  if (should_show_suggestion_chip_ == show) {
    return;
  }
  should_show_suggestion_chip_ = show;
  NotifyChange(Property::kShouldShowSuggestionChip);
}

void PageActionModel::SetSuggestionChipConfig(
    PageActionPassKey,
    const SuggestionChipConfig& config) {
  if (should_animate_ == config.should_animate &&
      should_announce_chip_ == config.should_announce_chip) {
    return;
  }
  should_animate_ = config.should_animate;
  should_announce_chip_ = config.should_announce_chip;
  NotifyChange(Property::kSuggestionChipConfig);
}

void PageActionModel::SetTabActive(PageActionPassKey, bool is_active) {
  if (is_tab_active_ == is_active) {
    return;
  }
  is_tab_active_ = is_active;
  NotifyChange(Property::kTabActive);
}

void PageActionModel::SetHasPinnedIcon(PageActionPassKey,
                                       bool has_pinned_icon) {
  if (has_pinned_icon_ == has_pinned_icon) {
    return;
  }
  has_pinned_icon_ = has_pinned_icon;
  NotifyChange(Property::kHasPinnedIcon);
}

void PageActionModel::SetActionItemProperties(PageActionPassKey,
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
    NotifyChange(Property::kActionItemProperties);
  }
}

bool PageActionModel::GetVisible() const {
  const bool hidden_by_omnibox =
      is_suppressed_by_omnibox_ && !is_exempt_from_omnibox_suppression_;

  return is_tab_active_ && !hidden_by_omnibox && action_item_enabled_ &&
         action_item_visible_ && show_requested_ && !has_pinned_icon_;
}

bool PageActionModel::IsChipShowing() const {
  return is_chip_showing_;
}

bool PageActionModel::ShouldShowSuggestionChip() const {
  return should_show_suggestion_chip_;
}

bool PageActionModel::GetShouldAnimateChipOut() const {
  return should_animate_;
}

bool PageActionModel::GetShouldAnimateChipIn() const {
  // Only animate in if the chip was not shown yet.
  return should_animate_ && !did_show_chip_;
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

bool PageActionModel::GetActionActive() const {
  return action_active_;
}

PageActionColorSource PageActionModel::GetColorSource() const {
  return color_source_.has_value() ? *color_source_
                                   : PageActionColorSource::kForeground;
}

void PageActionModel::SetOverrideText(
    PageActionPassKey,
    const std::optional<std::u16string>& override_text) {
  if (override_text_ == override_text) {
    return;
  }
  override_text_ = override_text;
  NotifyChange(Property::kOverrideText);
}

void PageActionModel::SetOverrideAccessibleName(
    PageActionPassKey,
    const std::optional<std::u16string>& override_accessible_name) {
  if (override_accessible_name_ == override_accessible_name) {
    return;
  }
  override_accessible_name_ = override_accessible_name;
  NotifyChange(Property::kOverrideAccessibleName);
}

void PageActionModel::SetOverrideImage(
    PageActionPassKey,
    const std::optional<ui::ImageModel>& override_image,
    PageActionColorSource color_source) {
  if (override_image_ == override_image && color_source == color_source_) {
    return;
  }
  override_image_ = override_image;
  color_source_ = color_source;
  NotifyChange(Property::kOverrideImage);
}

void PageActionModel::SetOverrideTooltip(
    PageActionPassKey,
    const std::optional<std::u16string>& override_tooltip) {
  if (override_tooltip_ == override_tooltip) {
    return;
  }
  override_tooltip_ = override_tooltip;
  NotifyChange(Property::kOverrideTooltip);
}

void PageActionModel::SetIsSuppressedByOmnibox(PageActionPassKey,
                                               bool is_suppressed) {
  if (is_suppressed_by_omnibox_ == is_suppressed) {
    return;
  }
  is_suppressed_by_omnibox_ = is_suppressed;
  NotifyChange(Property::kIsSuppressedByOmnibox);
}

void PageActionModel::SetExemptFromOmniboxSuppression(PageActionPassKey,
                                                      bool is_exempt) {
  if (is_exempt_from_omnibox_suppression_ == is_exempt) {
    return;
  }
  is_exempt_from_omnibox_suppression_ = is_exempt;
  NotifyChange(Property::kExemptFromOmniboxSuppression);
}

void PageActionModel::SetIsChipShowing(PageActionPassKey,
                                       bool is_chip_showing) {
  did_show_chip_ |= is_chip_showing;
  if (is_chip_showing_ == is_chip_showing) {
    return;
  }

  is_chip_showing_ = is_chip_showing;
  NotifyChange(Property::kIsChipShowing);
}

void PageActionModel::SetActionActive(PageActionPassKey, bool is_active) {
  if (action_active_ == is_active) {
    return;
  }

  action_active_ = is_active;
  NotifyChange(Property::kActionActive);
}

void PageActionModel::AddObserver(PageActionModelObserver* observer) {
  observer_list_.AddObserver(observer);
}

void PageActionModel::RemoveObserver(PageActionModelObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void PageActionModel::NotifyChange(Property property) {
  // Crash if the same property is modified again during notification, as this
  // would cause an infinite loop (A notifies -> observer sets A -> notifies
  // -> ...).
  CHECK(!notified_properties_.Has(property));
  notified_properties_.Put(property);

  if (is_notifying_observers_) {
    notified_properties_.Remove(property);
    return;
  }

  base::AutoReset<bool> auto_reset(&is_notifying_observers_, true);
  observer_list_.Notify(&PageActionModelObserver::OnPageActionModelChanged,
                        *this);

  notified_properties_.Remove(property);
}

bool PageActionModel::IsEphemeral() const {
  return is_ephemeral_;
}

void PageActionModel::SetShouldShowAnchoredMessage(PageActionPassKey,
                                                   bool show) {
  if (should_show_anchored_message_ == show) {
    return;
  }
  should_show_anchored_message_ = show;
  NotifyChange(Property::kShouldShowAnchoredMessage);
}

void PageActionModel::SetAnchoredMessageText(
    PageActionPassKey,
    const std::u16string& anchored_message) {
  if (anchored_message_text_ == anchored_message) {
    return;
  }
  anchored_message_text_ = anchored_message;
  NotifyChange(Property::kAnchoredMessageText);
}

void PageActionModel::SetAnchoredMessageAction(
    PageActionPassKey,
    const AnchoredMessageActionIconType action_icon_type,
    std::unique_ptr<ui::SimpleMenuModel> model) {
  anchored_message_action_icon_type_ = action_icon_type;
  std::unique_ptr<ui::SimpleMenuModel> old_model =
      std::move(anchored_message_menu_model_);
  anchored_message_menu_model_ = std::move(model);
  NotifyChange(Property::kAnchoredMessageActionIcon);
}

void PageActionModel::SetAnchoredMessageIcon(
    PageActionPassKey,
    const std::optional<ui::ImageModel>& icon) {
  anchored_message_icon_ = icon;
  NotifyChange(Property::kAnchoredMessageIcon);
}

bool PageActionModel::ShouldShowAnchoredMessage() const {
  return should_show_anchored_message_;
}

bool PageActionModel::IsAnchoredMessageShowing() const {
  return is_anchored_message_showing_;
}

void PageActionModel::SetIsAnchoredMessageShowing(
    PageActionPassKey,
    bool is_anchored_message_showing) {
  if (is_anchored_message_showing_ == is_anchored_message_showing) {
    return;
  }
  is_anchored_message_showing_ = is_anchored_message_showing;
  NotifyChange(Property::kIsAnchoredMessageShowing);
}

const std::u16string& PageActionModel::GetAnchoredMessageText() const {
  return anchored_message_text_;
}

AnchoredMessageActionIconType
PageActionModel::GetAnchoredMessageActionIconType() const {
  return anchored_message_action_icon_type_;
}

ui::SimpleMenuModel* PageActionModel::GetAnchoredMessageMenuModel() const {
  return anchored_message_menu_model_.get();
}

const std::optional<ui::ImageModel>& PageActionModel::GetAnchoredMessageIcon()
    const {
  return anchored_message_icon_;
}

}  // namespace page_actions
