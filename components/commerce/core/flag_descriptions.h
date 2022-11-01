// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_FLAG_DESCRIPTIONS_H_
#define COMPONENTS_COMMERCE_CORE_FLAG_DESCRIPTIONS_H_

namespace commerce::flag_descriptions {

// Enables the user to track prices of the Shopping URLs they are visiting.
// The first variation is to display price drops in the Tab Switching UI when
// they are identified.
extern const char kCommercePriceTrackingName[];
extern const char kCommercePriceTrackingDescription[];

extern const char kShoppingListName[];
extern const char kShoppingListDescription[];

extern const char kChromeCartDomBasedHeuristicsName[];
extern const char kChromeCartDomBasedHeuristicsDescription[];

}  // namespace commerce::flag_descriptions

#endif  // COMPONENTS_COMMERCE_CORE_FLAG_DESCRIPTIONS_H_
