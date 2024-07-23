// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/performance_controls/tab_list_model.h"

#include <algorithm>
#include <vector>

#include "components/performance_manager/public/resource_attribution/page_context.h"

TabListModel::TabListModel(
    const std::vector<resource_attribution::PageContext>& page_contexts)
    : page_contexts_(page_contexts) {}

TabListModel::~TabListModel() = default;

void TabListModel::AddObserver(Observer* observer) {
  tab_list_model_observers_.AddObserver(observer);
}

void TabListModel::RemoveObserver(Observer* observer) {
  tab_list_model_observers_.RemoveObserver(observer);
}

void TabListModel::RemovePageContext(resource_attribution::PageContext tab) {
  auto it = std::find(page_contexts_.begin(), page_contexts_.end(), tab);
  if (it != page_contexts_.end()) {
    page_contexts_.erase(it);

    for (auto& observer : tab_list_model_observers_) {
      observer.OnTabCountChanged(page_contexts_.size());
    }
  }
}

std::vector<resource_attribution::PageContext> TabListModel::page_contexts() {
  return page_contexts_;
}

int TabListModel::count() {
  return page_contexts_.size();
}
