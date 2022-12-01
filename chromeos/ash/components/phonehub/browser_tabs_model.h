// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_BROWSER_TABS_MODEL_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_BROWSER_TABS_MODEL_H_

#include <ostream>
#include <string>

#include "base/time/time.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace ash {
namespace phonehub {

// Contains metadata about browser tabs that are open on the user's phone.
class BrowserTabsModel {
 public:
  static const size_t kMaxMostRecentTabs;

  struct BrowserTabMetadata {
    BrowserTabMetadata(GURL url,
                       const std::u16string& title,
                       base::Time last_accessed_timestamp,
                       const gfx::Image& favicon);
    BrowserTabMetadata(const BrowserTabMetadata& other);
    BrowserTabMetadata& operator=(const BrowserTabMetadata& other);

    bool operator==(const BrowserTabMetadata& other) const;
    bool operator!=(const BrowserTabMetadata& other) const;
    bool operator<(const BrowserTabMetadata& other) const;

    GURL url;
    std::u16string title;
    base::Time last_accessed_timestamp;
    gfx::Image favicon;
  };

  // |is_tab_sync_enabled| indicates whether the Chrome OS device is currently
  // syncing tab metadata. If that parameter is false, |most_recent_tabs_|
  // should be empty. If it is true, |most_recent_tabs_| can contain up to four.
  BrowserTabsModel(
      bool is_tab_sync_enabled,
      const std::vector<BrowserTabMetadata>& most_recent_tabs = {});
  BrowserTabsModel(const BrowserTabsModel& other);
  ~BrowserTabsModel();

  bool operator==(const BrowserTabsModel& other) const;
  bool operator!=(const BrowserTabsModel& other) const;

  bool is_tab_sync_enabled() const { return is_tab_sync_enabled_; }

  const std::vector<BrowserTabMetadata>& most_recent_tabs() const {
    return most_recent_tabs_;
  }

 private:
  bool is_tab_sync_enabled_;

  // Sorted from most recently visited to least recently visited.
  std::vector<BrowserTabMetadata> most_recent_tabs_;
};

std::ostream& operator<<(
    std::ostream& stream,
    BrowserTabsModel::BrowserTabMetadata browser_tab_metadata);

}  // namespace phonehub
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_BROWSER_TABS_MODEL_H_
