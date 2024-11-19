// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/page_action_model.h"

#include "chrome/browser/ui/views/page_action/page_action_model_observer.h"

namespace page_actions {

PageActionModel::PageActionModel() = default;
PageActionModel::~PageActionModel() = default;

void PageActionModel::SetVisible(bool visible) {
  if (is_visible_ == visible) {
    return;
  }
  is_visible_ = visible;
  observer_list_.Notify(&PageActionModelObserver::OnVisibleChanged,
                        is_visible_);
}

void PageActionModel::AddObserver(PageActionModelObserver* observer) {
  observer_list_.AddObserver(observer);
}

void PageActionModel::RemoveObserver(PageActionModelObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

}  // namespace page_actions
