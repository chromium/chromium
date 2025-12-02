// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.account_picker;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.signin.services.SigninFlowTimestampsLogger.FlowVariant;
import org.chromium.components.signin.base.CoreAccountInfo;

/**
 * This interface abstracts the sign-in logic for the account picker bottom sheet. There is one
 * implementation per {@link EntryPoint}.
 */
@NullMarked
public interface AccountPickerDelegate {

    /** A controller for the state of the sign-in flow, e.g. showing error screens. */
    interface SigninStateController {

        /** Shows the sign-in flow generic error state. */
        void showGenericError();

        /** Show the sign-in flow auth error state. */
        void showAuthError();

        /** Must be called when the sign-in flow finishes. */
        void onSigninComplete();
    }

    /** Releases resources used by this class. */
    void onAccountPickerDestroy();

    /**
     * Returns whether the "add account" action is handled by the delegate. TODO(b/326019991):
     * Remove the method once all bottom sheet entry points will be started from
     * `SigninAndHistorySyncActivity`.
     */
    boolean canHandleAddAccount();

    /**
     * Called when the user triggers the "add account" action the sign-in bottom sheet. Triggers the
     * "add account" flow in the embedder.
     */
    void addAccount();

    /** Called when the current signed-in account is signed-out prior to the sign-in operation. */
    default void onSignoutBeforeSignin() {}

    /** Called when the sign-in finishes successfully. */
    void onSignInComplete(
            CoreAccountInfo accountInfo, AccountPickerDelegate.SigninStateController controller);

    /**
     * Called when the seamless sign-in process cannot proceed, for example, if the target account
     * is removed. Implementers should use this to clean up resources and ensure any associated UI
     * is dismissed.
     * TODO(crbug.com/464507068): This method name is temporary and linked to a specific
     * implementation. The interface should be improved to use a generic `onSignInCancel()` from the
     * delegate.
     */
    default void onSeamlessSigninAbandoned() {}

    default @FlowVariant String getSigninFlowVariant() {
        return FlowVariant.OTHER;
    }
}
