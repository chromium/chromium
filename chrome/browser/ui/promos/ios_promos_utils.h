// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PROMOS_IOS_PROMOS_UTILS_H_
#define CHROME_BROWSER_UI_PROMOS_IOS_PROMOS_UTILS_H_

#include "base/functional/callback_forward.h"
#include "components/desktop_to_mobile_promos/promos_types.h"

class Browser;
class Profile;

namespace ios_promos_utils {

// VerifyIOSPromoEligibility gets the classification/eligibility result from the
// segmentation platform and then asynchronously calls
// `OnIOSPromoClassificationResult` to determine whether or not the user should
// be shown the promo.
void VerifyIOSPromoEligibility(
    desktop_to_mobile_promos::PromoType promo_type,
    Browser* browser,
    desktop_to_mobile_promos::BubbleType bubble_type =
        desktop_to_mobile_promos::BubbleType::kQRCode);

// Checks if the user should be shown the iOS Payment promo and attempts to show
// it. This should only be called if a card was successfully uploaded and
// the VCN flow callback is null. Calls the correct callback depending on
// whether the promo will be shown or not.
void MaybeOverrideCardConfirmationBubbleWithIOSPaymentPromo(
    Browser* browser,
    base::OnceClosure promo_shown_callback,
    base::OnceClosure promo_not_shown_callback);

// Returns true if the signed-in user has been active 16 out of the last 28 days
// on an iOS device.
bool IsUserActiveOnIOS(Profile* profile);

// Returns true if the user has an Android device that has been active in the
// last 28 days. This is not exactly an Android version of
// `IsUserActiveOnIOS()` - the logic is different.
bool IsUserActiveOnAndroid(Profile* profile);

}  // namespace ios_promos_utils

#endif  // CHROME_BROWSER_UI_PROMOS_IOS_PROMOS_UTILS_H_
