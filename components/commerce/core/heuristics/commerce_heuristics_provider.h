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
// Check if the `width` and `height` could be the spec of an AddToCart button.
bool IsAddToCartButtonSpec(int height, int width);
// Check if the `tag` of a web element could be the tag of an AddToCart button.
bool IsAddToCartButtonTag(const std::string& tag);
// Check if the `text` of a web element could be the text of an AddToCart
// button.
bool IsAddToCartButtonText(const std::string& text);
// Check if we should use DOM-based heuristics for `url`.
bool ShouldUseDOMBasedHeuristics(const GURL& url);

}  // namespace commerce_heuristics

#endif  // COMPONENTS_COMMERCE_CORE_HEURISTICS_COMMERCE_HEURISTICS_PROVIDER_H_
