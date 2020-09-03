// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PHONEHUB_BROWSER_TABS_MODEL_H_
#define CHROMEOS_COMPONENTS_PHONEHUB_BROWSER_TABS_MODEL_H_

#include <ostream>

#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace chromeos {
namespace phonehub {

// Contains metadata about browser tabs that are open on the user's phone.
class BrowserTabsModel {
 public:
  struct BrowserTabMetadata {
    BrowserTabMetadata(GURL url,
                       const base::string16& title,
                       base::Time last_accessed_timestamp,
                       const gfx::Image& favicon);
    BrowserTabMetadata(const BrowserTabMetadata& other);

    bool operator==(const BrowserTabMetadata& other) const;
    bool operator!=(const BrowserTabMetadata& other) const;
    bool operator<(const BrowserTabMetadata& other) const;

    GURL url;
    base::string16 title;
    base::Time last_accessed_timestamp;
    gfx::Image favicon;
  };

  // |is_tab_sync_enabled| indicates whether the Chrome OS device is currently
  // syncing tab metadata. If that parameter is false, the optional tab
  // parameters should be null. If it is true, one or both of the parameters may
  // still be null if the user does not have browser tabs open on their phone.
  BrowserTabsModel(
      bool is_tab_sync_enabled,
      const base::Optional<BrowserTabMetadata>& most_recent_tab = base::nullopt,
      const base::Optional<BrowserTabMetadata>& second_most_recent_tab =
          base::nullopt);
  BrowserTabsModel(const BrowserTabsModel& other);
  ~BrowserTabsModel();

  bool operator==(const BrowserTabsModel& other) const;
  bool operator!=(const BrowserTabsModel& other) const;

  bool is_tab_sync_enabled() const { return is_tab_sync_enabled_; }
  const base::Optional<BrowserTabMetadata>& most_recent_tab() const {
    return most_recent_tab_;
  }
  const base::Optional<BrowserTabMetadata>& second_most_recent_tab() const {
    return second_most_recent_tab_;
  }

 private:
  bool is_tab_sync_enabled_;
  base::Optional<BrowserTabMetadata> most_recent_tab_;
  base::Optional<BrowserTabMetadata> second_most_recent_tab_;
};

std::ostream& operator<<(
    std::ostream& stream,
    BrowserTabsModel::BrowserTabMetadata browser_tab_metadata);

}  // namespace phonehub
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PHONEHUB_BROWSER_TABS_MODEL_H_
