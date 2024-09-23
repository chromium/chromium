// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_COMMERCE_UTILS_H_
#define COMPONENTS_COMMERCE_CORE_COMMERCE_UTILS_H_

#include "base/feature_list.h"
#include "components/commerce/core/commerce_types.h"

class GURL;

namespace base {
class Uuid;
}  // namespace base

namespace commerce {
// Returns whether the `url` contains the discount utm tags.
bool UrlContainsDiscountUtmTag(const GURL& url);

// Gets test data for the parcel tracking APIs if the |kParcelTrackingTestData|
// flag is enabled.
ParcelTrackingStatus GetParcelTrackingStatusTestData();

// Gets the url for the ProductSpec page based on `urls`.
GURL GetProductSpecsTabUrl(const std::vector<GURL>& urls);

// Gets the url for the ProductSpec page based on `uuid`.
GURL GetProductSpecsTabUrlForID(const base::Uuid& uuid);
}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_COMMERCE_UTILS_H_
