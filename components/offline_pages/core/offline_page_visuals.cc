// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/offline_page_visuals.h"

#include <iostream>
#include "base/base64.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "components/offline_pages/core/offline_store_utils.h"

namespace offline_pages {

OfflinePageVisuals::OfflinePageVisuals() = default;
OfflinePageVisuals::OfflinePageVisuals(int64_t id,
                                       base::Time in_expiration,
                                       const std::string& in_thumbnail,
                                       const std::string& in_favicon)
    : offline_id(id),
      expiration(in_expiration),
      thumbnail(in_thumbnail),
      favicon(in_favicon) {}
OfflinePageVisuals::OfflinePageVisuals(const OfflinePageVisuals& other) =
    default;
OfflinePageVisuals::OfflinePageVisuals(OfflinePageVisuals&& other) = default;
OfflinePageVisuals::~OfflinePageVisuals() {}

bool OfflinePageVisuals::operator==(const OfflinePageVisuals& other) const {
  return offline_id == other.offline_id && expiration == other.expiration &&
         thumbnail == other.thumbnail && favicon == other.favicon;
}

bool OfflinePageVisuals::operator<(const OfflinePageVisuals& other) const {
  return offline_id < other.offline_id;
}

}  // namespace offline_pages
