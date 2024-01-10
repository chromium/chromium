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

// Content type for network request.
extern const char kContentType[];

// Http DELETE method.
extern const char kDeleteHttpMethod[];

// Empty data for POST request.
extern const char kEmptyPostData[];

// Http GET method.
extern const char kGetHttpMethod[];

// OAuth name used for network request.
extern const char kOAuthName[];

// OAuth scope used for network request.
extern const char kOAuthScope[];

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

// Http POST method.
extern const char kPostHttpMethod[];

// The conversion multiplier to go from standard currency units to
// micro-currency units.
extern const long kToMicroCurrency;

// Please do not use below UTM constants beyond commerce use cases.
// UTM campaign label.
extern const char kUTMCampaignLabel[];

// UTM campaign value for partner merchant carts when discount is enabled.
extern const char kUTMCampaignValueForCartDiscount[];

// UTM campaign value for partner merchant carts when discount is disabled.
extern const char kUTMCampaignValueForCartNoDiscount[];

// UTM campaign value for non-partner merchant carts.
extern const char kUTMCampaignValueForChromeCart[];

// UTM campaign value for discounts in history clusters.
extern const char kUTMCampaignValueForDiscounts[];

// UTM medium label.
extern const char kUTMMediumLabel[];

// General UTM medium value.
extern const char kUTMMediumValue[];

// Prefix of UTM labels, including the underscore.
extern const char kUTMPrefix[];

// UTM source label.
extern const char kUTMSourceLabel[];

// General UTM source value.
extern const char kUTMSourceValue[];
}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_COMMERCE_CONSTANTS_H_
