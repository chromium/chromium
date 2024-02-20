// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.account_picker;

import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetCoordinator.EntryPoint;
import org.chromium.components.signin.base.CoreAccountInfo;

/**
 * This interface abstracts the sign-in logic for the account picker bottom sheet. There is one
 * implementation per {@link EntryPoint}.
 */
public interface AccountPickerDelegate {
    /** Releases resources used by this class. */
    void onAccountPickerDestroy();

    /**
     * Signs in the user with the given accountInfo. The provided mediator can be used to control
     * the behavior of the bottom sheet in response to failures, etc.
     */
    void signIn(CoreAccountInfo accountInfo, AccountPickerBottomSheetMediator mediator);

    /** Returns the entry point of this delegate. */
    @EntryPoint
    int getEntryPoint();
}
