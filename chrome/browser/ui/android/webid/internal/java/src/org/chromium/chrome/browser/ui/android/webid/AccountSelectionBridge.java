// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.ui.android.webid.data.Account;
import org.chromium.ui.base.WindowAndroid;

/**
 * This bridge creates and initializes a {@link AccountSelectionComponent} on construction and
 * forwards native calls to it.
 */
class AccountSelectionBridge implements AccountSelectionComponent.Delegate {
    private long mNativeView;

    @CalledByNative
    private AccountSelectionBridge(long nativeView, WindowAndroid windowAndroid) {
        mNativeView = nativeView;
    }

    @CalledByNative
    private void destroy() {
        mNativeView = 0;
    }

    /* Shows the accounts in a bottom sheet UI allowing user to select one.
     *
     * @param url is the URL for RP that has initiated the WebID flow.
     * @param accountsFiled is a 2d string array where each of its element is
     * the fields for a single account. Note that we use a string array to
     * represent and account given that it helps reduce the number of JNI
     * methods needed and thus avoid APK size overhead.
     */
    @CalledByNative
    private void showAccounts(String url, String[][] accountsFields) {
        Account[] accounts = new Account[accountsFields.length];
        for (int i = 0; i < accountsFields.length; i++) {
            String[] fields = accountsFields[i];
            accounts[i] = new Account(
                    /* subject= */ fields[0],
                    /* email= */ fields[1],
                    /* name= */ fields[2],
                    /* givenName= */ fields[3],
                    /* picture= */ fields[4],
                    /* originUrl= */ fields[5]);
        }

        // TODO(majidvp): Actually show UI that lists the account.
        onAccountSelected(accounts[0]);
    }

    @Override
    public void onDismissed() {
        if (mNativeView != 0) {
            AccountSelectionBridgeJni.get().onDismiss(mNativeView);
        }
    }

    @Override
    public void onAccountSelected(Account account) {
        if (mNativeView != 0) {
            AccountSelectionBridgeJni.get().onAccountSelected(mNativeView, account.getFields());
        }
    }

    @NativeMethods
    interface Natives {
        void onAccountSelected(long nativeAccountSelectionViewAndroid, String[] accountFields);
        void onDismiss(long nativeAccountSelectionViewAndroid);
    }
}
