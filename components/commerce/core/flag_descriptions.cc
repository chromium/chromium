// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/flag_descriptions.h"

namespace commerce::flag_descriptions {

const char kProductSpecificationsName[] = "Product Specifications";
const char kProductSpecificationsDescription[] =
    "Enable the Product Specifications feature.";

const char kShoppingListName[] = "Shopping List";
const char kShoppingListDescription[] = "Enable shopping list in bookmarks.";

const char kPriceInsightsName[] = "Price Insights";
const char kPriceInsightsDescription[] = "Enable price insights experiment.";

const char kDiscountOnNavigationName[] = "Discounts on navigation";
const char kDiscountOnNavigationDescription[] =
    "Enable to show available discounts on the page after navigation.";

const char kPriceTrackingSubscriptionServiceLocaleKeyName[] =
    "Price Tracking Subscription Service Local Key";
const char kPriceTrackingSubscriptionServiceLocaleKeyDescription[] =
    "Enable the locale key for price tracking subscription service";

const char kPriceTrackingSubscriptionServiceProductVersionName[] =
    "Price Tracking Subscription Service Product Version";
const char kPriceTrackingSubscriptionServiceProductVersionDescription[] =
    "Enable the product version logging for price tracking subscription "
    "service";

const char kDiscountAutofillName[] = "Discount Autofill";
const char kDiscountAutofillDescription[] =
    "Enable discount autofill experiment.";

const char kShoppingAlternateServerName[] = "Alternate Shopping Server.";
const char kShoppingAlternateServerDescription[] =
    "Enable using the alternate shopping server for testing.";

}  // namespace commerce::flag_descriptions
