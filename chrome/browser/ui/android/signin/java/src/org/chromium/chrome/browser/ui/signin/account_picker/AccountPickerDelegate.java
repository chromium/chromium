// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.account_picker;

import org.chromium.base.Callback;
import org.chromium.components.signin.base.CoreAccountInfo;

/**
 * This interface abstracts the sign-in logic for the account picker bottom sheet. There is one
 * implementation per {@link EntryPoint}.
 */
public interface AccountPickerDelegate {
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

    /**
     * Signs in the user with the given accountInfo. The provided mediator can be used to control
     * the behavior of the bottom sheet in response to failures, etc.
     */
    void signIn(CoreAccountInfo accountInfo, AccountPickerBottomSheetMediator mediator);

    /** Calls the callback with the result of SigninManager#isAccountManaged(). */
    void isAccountManaged(CoreAccountInfo accountInfo, Callback<Boolean> callback);

    /** See SigninManager#setUserAcceptedAccountManagement. */
    void setUserAcceptedAccountManagement(boolean confirmed);

    /** See SigninManager#extractDomainName. */
    String extractDomainName(String accountEmail);
}
