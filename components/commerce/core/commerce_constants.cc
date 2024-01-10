// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/commerce_constants.h"

namespace commerce {

const char kChromeUICommerceInternalsHost[] = "commerce-internals";

const char kChromeUIShoppingInsightsSidePanelHost[] =
    "shopping-insights-side-panel.top-chrome";

const char kChromeUIShoppingInsightsSidePanelUrl[] =
    "chrome://shopping-insights-side-panel.top-chrome";

const char kContentType[] = "application/json; charset=UTF-8";

const char kDeleteHttpMethod[] = "DELETE";

const char kEmptyPostData[] = "";

const char kGetHttpMethod[] = "GET";

const char kOAuthName[] = "chromememex_svc";

const char kOAuthScope[] = "https://www.googleapis.com/auth/chromememex";

const char kOgImage[] = "image";
const char kOgPriceAmount[] = "price:amount";
const char kOgPriceCurrency[] = "price:currency";
const char kOgProductLink[] = "product_link";
const char kOgTitle[] = "title";
const char kOgType[] = "type";

const char kOgTypeOgProduct[] = "product";
const char kOgTypeProductItem[] = "product.item";

const char kPostHttpMethod[] = "POST";

const long kToMicroCurrency = 1e6;

const char kUTMCampaignLabel[] = "utm_campaign";

const char kUTMCampaignValueForCartDiscount[] = "chrome-cart-discount-on";

const char kUTMCampaignValueForCartNoDiscount[] = "chrome-cart-discount-off";

const char kUTMCampaignValueForChromeCart[] = "chrome-cart";

const char kUTMCampaignValueForDiscounts[] =
    "chrome-history-cluster-with-discount";

const char kUTMMediumLabel[] = "utm_medium";

const char kUTMMediumValue[] = "app";

const char kUTMPrefix[] = "utm_";

const char kUTMSourceLabel[] = "utm_source";

const char kUTMSourceValue[] = "chrome";
}  // namespace commerce
