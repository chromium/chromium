// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/browser_tabs_model.h"

#include "chromeos/ash/components/multidevice/logging/logging.h"

namespace ash {
namespace phonehub {

const size_t BrowserTabsModel::kMaxMostRecentTabs = 2;

BrowserTabsModel::BrowserTabMetadata::BrowserTabMetadata(
    GURL url,
    const std::u16string& title,
    base::Time last_accessed_timestamp,
    const gfx::Image& favicon)
    : url(url),
      title(title),
      last_accessed_timestamp(last_accessed_timestamp),
      favicon(favicon) {}

BrowserTabsModel::BrowserTabMetadata::BrowserTabMetadata(
    const BrowserTabMetadata& other) = default;

BrowserTabsModel::BrowserTabMetadata&
BrowserTabsModel::BrowserTabMetadata::operator=(
    const BrowserTabMetadata& other) = default;

bool BrowserTabsModel::BrowserTabMetadata::operator==(
    const BrowserTabMetadata& other) const {
  // The favicon is not compared because equality of gfx::Image is defined
  // by the same storage space rather than the image itself.
  return url == other.url && title == other.title &&
         last_accessed_timestamp == other.last_accessed_timestamp;
}

bool BrowserTabsModel::BrowserTabMetadata::operator!=(
    const BrowserTabMetadata& other) const {
  return !(*this == other);
}

bool BrowserTabsModel::BrowserTabMetadata::operator<(
    const BrowserTabMetadata& other) const {
  // More recently visited tabs should come before earlier visited tabs.
  return std::tie(last_accessed_timestamp) >
         std::tie(other.last_accessed_timestamp);
}

BrowserTabsModel::BrowserTabsModel(
    bool is_tab_sync_enabled,
    const std::vector<BrowserTabMetadata>& most_recent_tabs)
    : is_tab_sync_enabled_(is_tab_sync_enabled),
      most_recent_tabs_(most_recent_tabs) {
  if (!is_tab_sync_enabled_ && !most_recent_tabs_.empty()) {
    PA_LOG(WARNING) << "Tab sync is not enabled, but tab metadata was "
                    << "provided; clearing metadata.";
    most_recent_tabs_.clear();
    return;
  }

  std::sort(most_recent_tabs_.begin(), most_recent_tabs_.end());
  if (most_recent_tabs_.size() > kMaxMostRecentTabs) {
    PA_LOG(WARNING) << "More than the max number of browser tab metadatas were "
                       "provided; truncating least recently visited browser "
                       "tabs' metadatas.";
    most_recent_tabs_.erase(most_recent_tabs_.begin() + kMaxMostRecentTabs,
                            most_recent_tabs_.end());
    return;
  }
}

BrowserTabsModel::BrowserTabsModel(const BrowserTabsModel& other) = default;

BrowserTabsModel::~BrowserTabsModel() = default;

bool BrowserTabsModel::operator==(const BrowserTabsModel& other) const {
  return is_tab_sync_enabled_ == other.is_tab_sync_enabled_ &&
         most_recent_tabs_ == other.most_recent_tabs_;
}

bool BrowserTabsModel::operator!=(const BrowserTabsModel& other) const {
  return !(*this == other);
}

std::ostream& operator<<(
    std::ostream& stream,
    BrowserTabsModel::BrowserTabMetadata browser_tab_metadata) {
  stream << "{URL: " << browser_tab_metadata.url.spec() << ", "
         << "Title: " << browser_tab_metadata.title << ", "
         << "Timestamp: " << browser_tab_metadata.last_accessed_timestamp
         << "}";
  return stream;
}

}  // namespace phonehub
}  // namespace ash
