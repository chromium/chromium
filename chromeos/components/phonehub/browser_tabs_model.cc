// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/browser_tabs_model.h"

#include "chromeos/components/multidevice/logging/logging.h"

namespace chromeos {
namespace phonehub {

BrowserTabsModel::BrowserTabMetadata::BrowserTabMetadata(
    GURL url,
    const base::string16& title,
    base::Time last_accessed_timestamp,
    const gfx::Image& favicon)
    : url(url),
      title(title),
      last_accessed_timestamp(last_accessed_timestamp),
      favicon(favicon) {}

BrowserTabsModel::BrowserTabMetadata::BrowserTabMetadata(
    const BrowserTabMetadata& other) = default;

bool BrowserTabsModel::BrowserTabMetadata::operator==(
    const BrowserTabMetadata& other) const {
  return url == other.url && title == other.title &&
         last_accessed_timestamp == other.last_accessed_timestamp &&
         favicon == other.favicon;
}

bool BrowserTabsModel::BrowserTabMetadata::operator!=(
    const BrowserTabMetadata& other) const {
  return !(*this == other);
}

bool BrowserTabsModel::BrowserTabMetadata::operator<(
    const BrowserTabMetadata& other) const {
  return std::tie(last_accessed_timestamp) <
         std::tie(other.last_accessed_timestamp);
}

BrowserTabsModel::BrowserTabsModel(
    bool is_tab_sync_enabled,
    const base::Optional<BrowserTabMetadata>& most_recent_tab,
    const base::Optional<BrowserTabMetadata>& second_most_recent_tab)
    : is_tab_sync_enabled_(is_tab_sync_enabled),
      most_recent_tab_(most_recent_tab),
      second_most_recent_tab_(second_most_recent_tab) {
  if (!is_tab_sync_enabled_ &&
      (most_recent_tab_.has_value() || second_most_recent_tab_.has_value())) {
    PA_LOG(WARNING) << "Tab sync is not enabled, but tab metadata was "
                    << "provided; clearing metadata.";
    most_recent_tab_.reset();
    second_most_recent_tab_.reset();
  }
}

BrowserTabsModel::BrowserTabsModel(const BrowserTabsModel& other) = default;

BrowserTabsModel::~BrowserTabsModel() = default;

bool BrowserTabsModel::operator==(const BrowserTabsModel& other) const {
  return is_tab_sync_enabled_ == other.is_tab_sync_enabled_ &&
         most_recent_tab_ == other.most_recent_tab_ &&
         second_most_recent_tab_ == other.second_most_recent_tab_;
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
}  // namespace chromeos
