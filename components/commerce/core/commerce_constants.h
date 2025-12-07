// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_COMMERCE_CONSTANTS_H_
#define COMPONENTS_COMMERCE_CORE_COMMERCE_CONSTANTS_H_

namespace commerce {

// The host for the commerce internals page.
inline constexpr char kChromeUICommerceInternalsHost[] = "commerce-internals";

// The host for compare.
inline constexpr char kChromeUICompareHost[] = "compare";

// The URL for compare.
inline constexpr char kChromeUICompareUrl[] = "chrome://compare";

// The URL for compare disclosure.
inline constexpr char kChromeUICompareDisclosureUrl[] =
    "chrome://compare/disclosure";

// The URL for compare learn more page.
inline constexpr char kChromeUICompareLearnMoreUrl[] =
    "https://support.google.com/chrome/?p=compare_tabs";

// The URL for compare learn more page for managed users.
inline constexpr char kChromeUICompareLearnMoreManagedUrl[] =
    "https://support.google.com/chrome/a?p=tab_compare";

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

// The maximum enforced interval (in days) between two triggers of the product
// specifications entry point.
inline constexpr int kProductSpecMaxEntryPointTriggeringInterval = 64;

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

// Prefix for model quality logging entry for product specifications.
inline constexpr char kProductSpecificationsLoggingPrefix[] =
    "product-specifications:";

// A means of specifying the URL for the product specifications backend from
// the command line.
inline constexpr char kProductSpecificationsUrlKey[] =
    "product-specifications-url";

// The conversion multiplier to go from standard currency units to
// micro-currency units.
inline constexpr long kToMicroCurrency = 1e6;

// Header name for using alternate shopping server.
inline constexpr char kAlternateServerHeaderName[] = "x-use-alt-service";
inline constexpr char kAlternateServerHeaderTrueValue[] = "true";

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_COMMERCE_CONSTANTS_H_
