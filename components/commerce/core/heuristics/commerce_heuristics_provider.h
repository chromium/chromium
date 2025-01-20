// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_HEURISTICS_COMMERCE_HEURISTICS_PROVIDER_H_
#define COMPONENTS_COMMERCE_CORE_HEURISTICS_COMMERCE_HEURISTICS_PROVIDER_H_

#include "url/gurl.h"

namespace commerce_heuristics {

// Check if a URL is a cart page URL.
bool IsVisitCart(const GURL& url);
// Check if a URL is a checkout page URL.
bool IsVisitCheckout(const GURL& url);

}  // namespace commerce_heuristics

#endif  // COMPONENTS_COMMERCE_CORE_HEURISTICS_COMMERCE_HEURISTICS_PROVIDER_H_
