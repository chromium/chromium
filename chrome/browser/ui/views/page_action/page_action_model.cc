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
  observer_list_.Notify(&PageActionModelObserver::OnPageActionModelChanged,
                        this);
}

void PageActionModel::AddObserver(PageActionModelObserver* observer) {
  observer_list_.AddObserver(observer);
}

void PageActionModel::RemoveObserver(PageActionModelObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

}  // namespace page_actions
