// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_REQUEST_HEADER_OFFLINE_PAGE_NAVIGATION_UI_DATA_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_REQUEST_HEADER_OFFLINE_PAGE_NAVIGATION_UI_DATA_H_

namespace offline_pages {

class OfflinePageNavigationUIData {
 public:
  explicit OfflinePageNavigationUIData(bool is_offline_page);

  OfflinePageNavigationUIData(const OfflinePageNavigationUIData&) = default;
  OfflinePageNavigationUIData& operator=(const OfflinePageNavigationUIData&) =
      default;
  OfflinePageNavigationUIData(OfflinePageNavigationUIData&&) noexcept = default;
  OfflinePageNavigationUIData& operator=(
      OfflinePageNavigationUIData&&) noexcept = default;

  bool is_offline_page() const { return is_offline_page_; }

 private:
  bool is_offline_page_ = false;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_REQUEST_HEADER_OFFLINE_PAGE_NAVIGATION_UI_DATA_H_
