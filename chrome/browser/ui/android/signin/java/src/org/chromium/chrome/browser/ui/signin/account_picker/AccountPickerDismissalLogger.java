// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.account_picker;

import androidx.annotation.MainThread;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils;
import org.chromium.chrome.browser.signin.services.SigninPreferencesManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.signin.metrics.AccountConsistencyPromoAction;
import org.chromium.components.signin.metrics.SigninAccessPoint;

@NullMarked
class AccountPickerDismissalLogger {

    private final @SigninAccessPoint int mSigninAccessPoint;
    private final boolean mIsWebSignin;

    AccountPickerDismissalLogger(@SigninAccessPoint int signinAccessPoint, boolean isWebSignin) {
        mSigninAccessPoint = signinAccessPoint;
        mIsWebSignin = isWebSignin;
    }

    /**
     * Logs dismissal metrics based on the given StateChangeReason, if this is considered a
     * dismissal made by the user.
     */
    @MainThread
    void logBottomSheetDismissal(@StateChangeReason int reason) {
        final @AccountConsistencyPromoAction int promoAction;

        if (reason == StateChangeReason.SWIPE) {
            promoAction = AccountConsistencyPromoAction.DISMISSED_SWIPE_DOWN;
        } else if (reason == StateChangeReason.BACK_PRESS) {
            promoAction = AccountConsistencyPromoAction.DISMISSED_BACK;
        } else if (reason == StateChangeReason.TAP_SCRIM) {
            promoAction = AccountConsistencyPromoAction.DISMISSED_SCRIM;
        } else {
            return; // Not a user-initiated reason.
        }

        logActiveDismissal(promoAction);
    }

    /** Log dismissal metrics when user clicks on the secondary CTA to dismiss the bottom sheet. */
    @MainThread
    void logDismissedButtonClick() {
        logActiveDismissal(AccountConsistencyPromoAction.DISMISSED_BUTTON);
    }

    private void logActiveDismissal(@AccountConsistencyPromoAction int promoAction) {
        SigninMetricsUtils.logAccountConsistencyPromoAction(promoAction, mSigninAccessPoint);
        if (mIsWebSignin) {
            SigninPreferencesManager.getInstance()
                    .incrementWebSigninAccountPickerActiveDismissalCount();
        }
    }
}
