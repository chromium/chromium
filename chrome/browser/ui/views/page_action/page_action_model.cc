// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/page_action_model.h"

#include "base/types/pass_key.h"
#include "chrome/browser/ui/views/page_action/page_action_model_observer.h"
#include "ui/actions/actions.h"

namespace page_actions {

namespace {
using ::actions::ActionItem;
}  // namespace

PageActionModel::PageActionModel() = default;

PageActionModel::~PageActionModel() {
  observer_list_.Notify(
      &PageActionModelObserver::OnPageActionModelWillBeDeleted, this);
}

void PageActionModel::SetShowRequested(base::PassKey<PageActionController>,
                                       bool requested) {
  if (show_requested_ == requested) {
    return;
  }
  show_requested_ = requested;
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
  return action_item_enabled_ && action_item_visible_ && show_requested_;
}

const ui::ImageModel& PageActionModel::GetImage() const {
  return action_item_image_;
}

const std::u16string PageActionModel::GetText() const {
  return override_text_.value_or(text_);
}

const std::u16string PageActionModel::GetTooltipText() const {
  return tooltip_;
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

void PageActionModel::AddObserver(PageActionModelObserver* observer) {
  observer_list_.AddObserver(observer);
}

void PageActionModel::RemoveObserver(PageActionModelObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void PageActionModel::NotifyChange() {
  observer_list_.Notify(&PageActionModelObserver::OnPageActionModelChanged,
                        this);
}

}  // namespace page_actions
