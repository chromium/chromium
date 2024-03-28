// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/product_specifications/product_specifications_set.h"

namespace commerce {

ProductSpecificationsSet::ProductSpecificationsSet(
    const std::string& uuid,
    const int64_t creation_time_usec_since_epoch,
    const int64_t update_time_usec_since_epoch,
    const std::vector<const GURL>& urls,
    const std::string& name)
    : uuid_(base::Uuid::ParseLowercase(uuid)),
      creation_time_(base::Time::FromMillisecondsSinceUnixEpoch(
          creation_time_usec_since_epoch)),
      update_time_(base::Time::FromMillisecondsSinceUnixEpoch(
          update_time_usec_since_epoch)),
      urls_(urls),
      name_(name) {
  DCHECK(!uuid.empty());
  for (const GURL& url : urls_) {
    DCHECK(url.is_valid());
  }
}

ProductSpecificationsSet::ProductSpecificationsSet(
    const ProductSpecificationsSet&) = default;

ProductSpecificationsSet::~ProductSpecificationsSet() = default;

}  // namespace commerce
