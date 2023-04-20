// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/flag_descriptions.h"

namespace commerce::flag_descriptions {

const char kCommercePriceTrackingName[] = "Price Tracking";
const char kCommercePriceTrackingDescription[] =
    "Allows users to track product prices through Chrome.";

const char kShoppingListName[] = "Shopping List";
const char kShoppingListDescription[] = "Enable shopping list in bookmarks.";

const char kChromeCartDomBasedHeuristicsName[] =
    "ChromeCart DOM-based heuristics";
const char kChromeCartDomBasedHeuristicsDescription[] =
    "Enable DOM-based heuristics for ChromeCart.";

const char kPriceTrackingChipExperimentName[] =
    "Price Tracking Chip Experiment";
const char kPriceTrackingChipExperimentDescription[] =
    "Enable price tracking chip experiment.";

}  // namespace commerce::flag_descriptions
