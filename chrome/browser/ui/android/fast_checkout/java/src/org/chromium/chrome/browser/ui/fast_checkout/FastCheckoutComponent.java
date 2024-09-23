// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.fast_checkout;

import android.content.Context;

import org.chromium.chrome.browser.ui.fast_checkout.data.FastCheckoutAutofillProfile;
import org.chromium.chrome.browser.ui.fast_checkout.data.FastCheckoutCreditCard;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

import java.util.List;

/**
 * This component supports the user during checkout flows by offering a pre-selection of addresses
 * and credit cards and autofilling the chosen data. Data is selected via a bottom sheet that
 * suppresses the keyboard until dismissed.
 */
public interface FastCheckoutComponent {
    /**
     * This delegate is called when the FastCheckout component is interacted with (e.g. dismissed or
     * suggestion selected).
     */
    interface Delegate {
        /**
         * Called when the user makes a selection in the FastCheckoutComponent. The component (i.e.
         * the bottom sheet) gets closed after selection.
         * @param profile The selected {@link FastCheckoutAutofillProfile}.
         * @param creditCard The selected {@link FastCheckoutCreditCard}.
         */
        void onOptionsSelected(
                FastCheckoutAutofillProfile profile, FastCheckoutCreditCard creditCard);

        /**
         * Called when the user dismisses the FastCheckoutComponent. Not called if an option was
         * selected.
         */
        void onDismissed();

        /** Opens the Autofill profile settings menu. */
        void openAutofillProfileSettings();

        /** Opens the credit card settings menu. */
        void openCreditCardSettings();
    }

    /**
     * Initializes the component.
     * @param context A {@link Context} to create views and retrieve resources.
     * @param sheetController A {@link BottomSheetController} used to show/hide the sheet.
     * @param delegate A {@link Delegate} that handles events.
     */
    void initialize(Context context, BottomSheetController sheetController, Delegate delegate);

    /** Displays the given options in a new bottom sheet. */
    void showOptions(
            List<FastCheckoutAutofillProfile> profiles, List<FastCheckoutCreditCard> creditCards);

    /** Hides the bottom sheet. No-op if the sheet is already hidden. */
    void destroy();
}
