// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/ios/ios_restore_live_tab.h"

#include "components/sessions/ios/ios_serialized_navigation_builder.h"

namespace sessions {

RestoreIOSLiveTab::RestoreIOSLiveTab(web::proto::NavigationStorage storage)
    : storage_(std::move(storage)) {}

RestoreIOSLiveTab::~RestoreIOSLiveTab() = default;

bool RestoreIOSLiveTab::IsInitialBlankNavigation() {
  return false;
}

int RestoreIOSLiveTab::GetCurrentEntryIndex() {
  if (storage_.items_size() == 0) {
    return -1;
  }

  return storage_.last_committed_item_index();
}

int RestoreIOSLiveTab::GetPendingEntryIndex() {
  return -1;
}

sessions::SerializedNavigationEntry RestoreIOSLiveTab::GetEntryAtIndex(
    int index) {
  const web::proto::NavigationItemStorage& item = storage_.items(index);
  return sessions::IOSSerializedNavigationBuilder::FromNavigationStorageItem(
      index, item);
}

sessions::SerializedNavigationEntry RestoreIOSLiveTab::GetPendingEntry() {
  return sessions::SerializedNavigationEntry();
}

int RestoreIOSLiveTab::GetEntryCount() {
  return storage_.items_size();
}

sessions::SerializedUserAgentOverride
RestoreIOSLiveTab::GetUserAgentOverride() {
  return sessions::SerializedUserAgentOverride();
}

const web::WebState* RestoreIOSLiveTab::GetWebState() const {
  return nullptr;
}

}  // namespace sessions
