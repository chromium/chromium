// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/request_header/offline_page_navigation_ui_data.h"

namespace offline_pages {

OfflinePageNavigationUIData::OfflinePageNavigationUIData() = default;

OfflinePageNavigationUIData::OfflinePageNavigationUIData(bool is_offline_page)
    : is_offline_page_(is_offline_page) {}

std::unique_ptr<OfflinePageNavigationUIData>
OfflinePageNavigationUIData::DeepCopy() const {
  std::unique_ptr<OfflinePageNavigationUIData> copy(
      new OfflinePageNavigationUIData());
  copy->is_offline_page_ = is_offline_page_;
  return copy;
}

}  // namespace offline_pages
