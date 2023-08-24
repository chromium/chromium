// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_FLAG_DESCRIPTIONS_H_
#define COMPONENTS_COMMERCE_CORE_FLAG_DESCRIPTIONS_H_

namespace commerce::flag_descriptions {

extern const char kCommerceLocalPDPDetectionName[];
extern const char kCommerceLocalPDPDetectionDescription[];

// Enables the user to track prices of the Shopping URLs they are visiting.
// The first variation is to display price drops in the Tab Switching UI when
// they are identified.
extern const char kCommercePriceTrackingName[];
extern const char kCommercePriceTrackingDescription[];

extern const char kPriceTrackingIconColorsName[];
extern const char kPriceTrackingIconColorsDescription[];

extern const char kShoppingCollectionName[];
extern const char kShoppingCollectionDescription[];

extern const char kShoppingListName[];
extern const char kShoppingListDescription[];

extern const char kShoppingListTrackByDefaultName[];
extern const char kShoppingListTrackByDefaultDescription[];

extern const char kChromeCartDomBasedHeuristicsName[];
extern const char kChromeCartDomBasedHeuristicsDescription[];

extern const char kPriceInsightsName[];
extern const char kPriceInsightsDescription[];

extern const char kShowDiscountOnNavigationName[];
extern const char kShowDiscountOnNavigationDescription[];

extern const char kPriceTrackingChipExperimentName[];
extern const char kPriceTrackingChipExperimentDescription[];

extern const char kShoppingPageTypesName[];
extern const char kShoppingPageTypesDescription[];

}  // namespace commerce::flag_descriptions

#endif  // COMPONENTS_COMMERCE_CORE_FLAG_DESCRIPTIONS_H_
