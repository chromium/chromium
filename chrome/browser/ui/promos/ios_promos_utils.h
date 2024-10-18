// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PROMOS_IOS_PROMOS_UTILS_H_
#define CHROME_BROWSER_UI_PROMOS_IOS_PROMOS_UTILS_H_

class Profile;
class ToolbarButtonProvider;

enum class IOSPromoType;

namespace ios_promos_utils {

// VerifyIOSPromoEligibility gets the classification/eligibility result from the
// segmentation platform and then asynchronously calls
// `OnIOSPromoClassificationResult` to determine whether or not the user should
// be shown the promo.
void VerifyIOSPromoEligibility(IOSPromoType promo_type,
                               Profile* profile,
                               ToolbarButtonProvider* toolbar_button_provider);

}  // namespace ios_promos_utils

#endif  // CHROME_BROWSER_UI_PROMOS_IOS_PROMOS_UTILS_H_
