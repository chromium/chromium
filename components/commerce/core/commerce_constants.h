// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_COMMERCE_CONSTANTS_H_
#define COMPONENTS_COMMERCE_CORE_COMMERCE_CONSTANTS_H_

namespace commerce {

// The host for the commerce internals page.
inline constexpr char kChromeUICommerceInternalsHost[] = "commerce-internals";

// The host for the shopping insights side panel page.
inline constexpr char kChromeUIShoppingInsightsSidePanelHost[] =
    "shopping-insights-side-panel.top-chrome";

// The url for the shopping insights side panel page.
inline constexpr char kChromeUIShoppingInsightsSidePanelUrl[] =
    "chrome://shopping-insights-side-panel.top-chrome";

// Content type for network request.
inline constexpr char kContentType[] = "application/json; charset=UTF-8";

// Http DELETE method.
inline constexpr char kDeleteHttpMethod[] = "DELETE";

// Empty data for POST request.
inline constexpr char kEmptyPostData[] = "";

// Http GET method.
inline constexpr char kGetHttpMethod[] = "GET";

// Open graph keys.
inline constexpr char kOgImage[] = "image";
inline constexpr char kOgPriceAmount[] = "price:amount";
inline constexpr char kOgPriceCurrency[] = "price:currency";
inline constexpr char kOgProductLink[] = "product_link";
inline constexpr char kOgTitle[] = "title";
inline constexpr char kOgType[] = "type";

// Specific open graph values we're interested in.
inline constexpr char kOgTypeOgProduct[] = "product";
inline constexpr char kOgTypeProductItem[] = "product.item";

// Http POST method.
inline constexpr char kPostHttpMethod[] = "POST";

// The conversion multiplier to go from standard currency units to
// micro-currency units.
inline constexpr long kToMicroCurrency = 1e6;

// Header name for using alternate shopping server.
inline constexpr char kAlternateServerHeaderName[] = "x-use-alt-service";
inline constexpr char kAlternateServerHeaderTrueValue[] = "true";

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_COMMERCE_CONSTANTS_H_
