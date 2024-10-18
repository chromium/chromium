// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PROMOS_IOS_PROMOS_UTILS_H_
#define CHROME_BROWSER_UI_PROMOS_IOS_PROMOS_UTILS_H_

#include "base/functional/callback_forward.h"

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

// Checks if the user should be shown the iOS Payment promo and attempts to show
// it. This should only be called if a card was successfully uploaded and
// the VCN flow callback is null. Calls the correct callback depending on
// whether the promo will be shown or not.
void MaybeOverrideCardConfirmationBubbleWithIOSPaymentPromo(
    Profile* profile,
    ToolbarButtonProvider* toolbar_button_provider,
    base::OnceClosure promo_shown_callback,
    base::OnceClosure promo_not_shown_callback);

}  // namespace ios_promos_utils

#endif  // CHROME_BROWSER_UI_PROMOS_IOS_PROMOS_UTILS_H_
