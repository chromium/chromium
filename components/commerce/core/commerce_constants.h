// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_COMMERCE_CONSTANTS_H_
#define COMPONENTS_COMMERCE_CORE_COMMERCE_CONSTANTS_H_

namespace commerce {

// The host for the commerce internals page.
extern const char kChromeUICommerceInternalsHost[];

// The host for the shopping insights side panel page.
extern const char kChromeUIShoppingInsightsSidePanelHost[];

// The url for the shopping insights side panel page.
extern const char kChromeUIShoppingInsightsSidePanelUrl[];

// Http DELETE method.
extern const char kDeleteHttpMethod[];

// Http GET method.
extern const char kGetHttpMethod[];

// Http POST method.
extern const char kPostHttpMethod[];

// OAuth name used for network request.
extern const char kOAuthName[];

// OAuth scope used for network request.
extern const char kOAuthScope[];

// Content type for network request.
extern const char kContentType[];

// Empty data for POST request.
extern const char kEmptyPostData[];

// Please do not use below UTM constants beyond commerce use cases.
// UTM source label.
extern const char kUTMSourceLabel[];

// UTM medium label.
extern const char kUTMMediumLabel[];

// UTM campaign label.
extern const char kUTMCampaignLabel[];

// General UTM source value.
extern const char kUTMSourceValue[];

// General UTM medium value.
extern const char kUTMMediumValue[];

// UTM campaign value for discounts in history clusters.
extern const char kUTMCampaignValueForDiscounts[];

// Prefix of UTM labels, including the underscore.
extern const char kUTMPrefix[];

// Open graph keys.
extern const char kOgImage[];
extern const char kOgPriceAmount[];
extern const char kOgPriceCurrency[];
extern const char kOgProductLink[];
extern const char kOgTitle[];
extern const char kOgType[];

// Specific open graph values we're interested in.
extern const char kOgTypeOgProduct[];
extern const char kOgTypeProductItem[];

// UTM tags related to ChromeCart and cart-related discounts.
// Tag for non-partner merchant carts.
extern const char kUTMCampaignValueForChromeCart[];

// Tag for partner merchant carts when discount is enabled.
extern const char kUTMCampaignValueForCartDiscount[];

// Tag for partner merchant carts when discount is disabled.
extern const char kUTMCampaignValueForCartNoDiscount[];
}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_COMMERCE_CONSTANTS_H_
