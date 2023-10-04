// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_COMMERCE_UTILS_H_
#define COMPONENTS_COMMERCE_CORE_COMMERCE_UTILS_H_

#include "base/feature_list.h"

class GURL;

namespace commerce {

// Feature flag for Discounts on navigation. This is supposed to be included in
// the commerce_feature_list file, but because of crbug.com/1155712 we have to
// move this feature flag here instead.
BASE_DECLARE_FEATURE(kShowDiscountOnNavigation);

// Returns whether the `url` contains the discount utm tags.
bool UrlContainsDiscountUtmTag(const GURL& url);

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_COMMERCE_UTILS_H_
