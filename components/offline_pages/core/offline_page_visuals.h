// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_VISUALS_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_VISUALS_H_

#include <stdint.h>
#include <iosfwd>
#include <string>

#include "base/time/time.h"

namespace offline_pages {

// Visuals for an offline page. This maps to a row in the page_thumbnails
// table.
struct OfflinePageVisuals {
 public:
  OfflinePageVisuals();
  OfflinePageVisuals(int64_t offline_id,
                     base::Time expiration,
                     const std::string& thumbnail,
                     const std::string& favicon);
  OfflinePageVisuals(const OfflinePageVisuals& other);
  OfflinePageVisuals(OfflinePageVisuals&& other);
  ~OfflinePageVisuals();
  OfflinePageVisuals& operator=(const OfflinePageVisuals& other) = default;
  bool operator==(const OfflinePageVisuals& other) const;
  bool operator<(const OfflinePageVisuals& other) const;
  std::string ToString() const;

  // The primary key/ID for the page in offline pages internal database.
  int64_t offline_id = 0;
  // The time at which the visuals can be removed from the table, but only
  // if the offline_id does not match an offline_id in the offline pages table.
  base::Time expiration;
  // The thumbnail raw image data.
  std::string thumbnail;
  // The favicon raw image data.
  std::string favicon;
};

std::ostream& operator<<(std::ostream& out, const OfflinePageVisuals& visuals);
}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_VISUALS_H_
