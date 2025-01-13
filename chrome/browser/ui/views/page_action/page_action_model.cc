// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/page_action_model.h"

#include "base/types/pass_key.h"
#include "chrome/browser/ui/views/page_action/page_action_model_observer.h"

namespace page_actions {
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

void PageActionModel::SetActionItemEnabled(base::PassKey<PageActionController>,
                                           bool enabled) {
  if (action_item_enabled_ == enabled) {
    return;
  }
  action_item_enabled_ = enabled;
  NotifyChange();
}

void PageActionModel::SetActionItemVisible(base::PassKey<PageActionController>,
                                           bool visible) {
  if (action_item_visible_ == visible) {
    return;
  }
  action_item_visible_ = visible;
  NotifyChange();
}

bool PageActionModel::GetVisible() const {
  return action_item_enabled_ && action_item_visible_ && show_requested_;
}

void PageActionModel::SetImage(const ui::ImageModel& image) {
  if (action_item_image_ == image) {
    return;
  }
  action_item_image_ = image;
  NotifyChange();
}
const ui::ImageModel& PageActionModel::GetImage() const {
  return action_item_image_;
}

void PageActionModel::SetText(const std::u16string& text) {
  if (text_ == text) {
    return;
  }
  text_ = text;
  NotifyChange();
}
const std::u16string PageActionModel::GetText() const {
  return override_text_.value_or(text_);
}

void PageActionModel::SetTooltipText(const std::u16string& tooltip) {
  if (tooltip_ == tooltip) {
    return;
  }
  tooltip_ = tooltip;
  NotifyChange();
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
