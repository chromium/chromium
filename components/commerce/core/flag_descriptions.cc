// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/flag_descriptions.h"

namespace commerce::flag_descriptions {

const char kCommerceLocalPDPDetectionName[] = "Local Product Page Detection";
const char kCommerceLocalPDPDetectionDescription[] =
    "Allow Chrome to attempt to detect product pages on the client, without "
    "server support.";

const char kProductSpecificationsName[] = "Product Specifications";
const char kProductSpecificationsDescription[] =
    "Enable the Product Specifications feature.";

const char kProductSpecificationsMultiSpecificsName[] =
    "Product Specifications Multi Specifics";
const char kProductSpecificationsMultiSpecificsDescription[] =
    "Enable the Product Specifications backed by the sync multi specifics "
    "representation.";

const char kCompareConfirmationToastName[] = "Added to set confirmation toast";
const char kCompareConfirmationToastDescription[] =
    "Enable to show the added to set confirmation in a toast.";

const char kCompareManagementInterfaceName[] =
    "Comparison table management from main Compare UI";
const char kCompareManagementInterfaceDescription[] =
    "Enable showing the comparison tables list in the Compare UI and enable "
    "the new \"Comparison tables\" context menu under \"Bookmarks and Lists\".";

const char kShoppingListName[] = "Shopping List";
const char kShoppingListDescription[] = "Enable shopping list in bookmarks.";

const char kChromeCartDomBasedHeuristicsName[] =
    "ChromeCart DOM-based heuristics";
const char kChromeCartDomBasedHeuristicsDescription[] =
    "Enable DOM-based heuristics for ChromeCart.";

const char kPriceInsightsName[] = "Price Insights";
const char kPriceInsightsDescription[] = "Enable price insights experiment.";

const char kDiscountOnNavigationName[] = "Discounts on navigation";
const char kDiscountOnNavigationDescription[] =
    "Enable to show available discounts on the page after navigation.";

#if BUILDFLAG(IS_IOS)
extern const char kPriceInsightsIosName[] = "Price Insights";
extern const char kPriceInsightsIosDescription[] =
    "When enabled, the user will be able to get price insights on product "
    "pages.";

extern const char kPriceInsightsHighPriceIosName[] =
    "Price Insights with high price";
extern const char kPriceInsightsHighPriceIosDescription[] =
    "When enabled, price insight will report a high confidence when the price "
    "is high.";
#endif

const char kShoppingPageTypesName[] = "Shopping Page Types";
const char kShoppingPageTypesDescription[] =
    "Enable shopping page types experiment.";

const char kTrackByDefaultOnMobileName[] =
    "Product Tracking by Default on Mobile";
const char kTrackByDefaultOnMobileDescription[] =
    "Enable tracking a product by default when bookmarking on mobile devices.";

const char kPriceTrackingSubscriptionServiceLocaleKeyName[] =
    "Price Tracking Subscription Service Local Key";
const char kPriceTrackingSubscriptionServiceLocaleKeyDescription[] =
    "Enable the locale key for price tracking subscription service";

const char kPriceTrackingSubscriptionServiceProductVersionName[] =
    "Price Tracking Subscription Service Product Version";
const char kPriceTrackingSubscriptionServiceProductVersionDescription[] =
    "Enable the product version logging for price tracking subscription "
    "service";

}  // namespace commerce::flag_descriptions
