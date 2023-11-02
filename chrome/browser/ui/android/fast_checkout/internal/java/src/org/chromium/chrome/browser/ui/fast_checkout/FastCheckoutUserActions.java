// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.fast_checkout;

import org.chromium.base.metrics.RecordUserAction;

/**
 * Defines Fast Checkout's UMA user actions. The resulting string returned by
 * `getAction()` needs to be documented at tools/metrics/actions/actions.xml.
 */
public enum FastCheckoutUserActions {
    INITIALIZED("Initialized"),
    ACCEPTED("Accepted"),
    NAVIGATED_BACK_HOME("NavigatedBackHome"),
    NAVIGATED_TO_ADDRESSES("NavigatedToAddresses"),
    NAVIGATED_TO_CREDIT_CARDS("NavigatedToCreditCards"),
    NAVIGATED_TO_ADDRESSES_SETTINGS_VIA_ICON("NavigatedToAddressesSettingsViaIcon"),
    NAVIGATED_TO_CREDIT_CARDS_SETTINGS_VIA_ICON("NavigatedToCreditCardsSettingsViaIcon"),
    NAVIGATED_TO_ADDRESSES_SETTINGS_VIA_FOOTER("NavigatedToAddressesSettingsViaFooter"),
    NAVIGATED_TO_CREDIT_CARDS_SETTINGS_VIA_FOOTER("NavigatedToCreditCardsSettingsViaFooter"),
    SELECTED_DIFFERENT_ADDRESS("SelectedDifferentAddress"),
    SELECTED_DIFFERENT_CREDIT_CARD("SelectedDifferentCreditCard"),
    SELECTED_SAME_ADDRESS("SelectedSameAddress"),
    SELECTED_SAME_CREDIT_CARD("SelectedSameCreditCard"),
    DISMISSED("Dismissed"),
    DESTROYED("Destroyed");

    private static final String USER_ACTION_PREFIX = "Autofill.FastCheckout.";

    private final String mAction;

    FastCheckoutUserActions(final String action) {
        this.mAction = action;
    }

    String getAction() {
        return USER_ACTION_PREFIX + mAction;
    }

    public void log() {
        RecordUserAction.record(getAction());
    }
}
