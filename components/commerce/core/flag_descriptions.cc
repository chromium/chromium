// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/flag_descriptions.h"

namespace commerce::flag_descriptions {

const char kCommerceLocalPDPDetectionName[] = "Local Product Page Detection";
const char kCommerceLocalPDPDetectionDescription[] =
    "Allow Chrome to attempt to detect product pages on the client, without "
    "server support.";

const char kCommercePriceTrackingName[] = "Price Tracking";
const char kCommercePriceTrackingDescription[] =
    "Allows users to track product prices through Chrome.";

const char kPriceTrackingIconColorsName[] =
    "Price Tracking Icon Tonal UI Colors";
const char kPriceTrackingIconColorsDescription[] =
    "Tonal colors for the expanded state of the price tracking chip on "
    "desktop.";

const char kShoppingIconColorVariantName[] =
    "Enable color variant for shopping icons";
const char kShoppingIconColorVariantDescription[] =
    "Enables a color variant for shopping page action icons (Price Insights & "
    "Price Tracking)";

const char kShoppingCollectionName[] = "Shopping Collection";
const char kShoppingCollectionDescription[] =
    "Organize all products into an automatically created bookmark folder.";

const char kShoppingListName[] = "Shopping List";
const char kShoppingListDescription[] = "Enable shopping list in bookmarks.";

const char kChromeCartDomBasedHeuristicsName[] =
    "ChromeCart DOM-based heuristics";
const char kChromeCartDomBasedHeuristicsDescription[] =
    "Enable DOM-based heuristics for ChromeCart.";

const char kParcelTrackingTestDataName[] = "Parcel Tracking Test Data";
const char kParcelTrackingTestDataDescription[] =
    "The parcel status API returns fake data for testing.";

const char kPriceInsightsName[] = "Price Insights";
const char kPriceInsightsDescription[] = "Enable price insights experiment.";

const char kShoppingPageTypesName[] = "Shopping Page Types";
const char kShoppingPageTypesDescription[] =
    "Enable shopping page types experiment.";

}  // namespace commerce::flag_descriptions
