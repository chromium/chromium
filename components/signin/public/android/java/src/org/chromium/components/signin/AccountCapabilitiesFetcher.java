// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.components.signin.base.AccountCapabilities;
import org.chromium.components.signin.base.CoreAccountInfo;

/**
 * AccountCapabilitiesFetcher retrieves account capabilities from {@link AccountManagerFacade} and
 * passes them to native. It has a C++ counterpart located in
 * account_capabilities_fetcher_android.{h,cc}.
 */
public class AccountCapabilitiesFetcher {
    private static final long INVALID_NATIVE_CALLBACK = 0;
    private final CoreAccountInfo mCoreAccountInfo;
    private long mNativeCallback;

    /**
     * Constructs a fetcher tied to a single account identified by {@code coreAccountInfo}.
     *
     * WARNING:
     * {@code nativeCallback} points to an unowned callback object allocated on native heap. {@code
     * this} *must* call {@link #onCapabilitiesFetchComplete} in order to clean up the callback
     * object properly.
     */
    @CalledByNative
    public AccountCapabilitiesFetcher(CoreAccountInfo coreAccountInfo, long nativeCallback) {
        assert nativeCallback != INVALID_NATIVE_CALLBACK;
        mCoreAccountInfo = coreAccountInfo;
        mNativeCallback = nativeCallback;
    }
    ;

    @CalledByNative
    public void startFetchingAccountCapabilities() {
        AccountManagerFacadeProvider.getInstance()
                .getAccountCapabilities(mCoreAccountInfo)
                .then(
                        accountCapabilities -> {
                            onCapabilitiesFetchComplete(accountCapabilities);
                        });
    }

    private void onCapabilitiesFetchComplete(AccountCapabilities accountCapabilities) {
        assert mNativeCallback != INVALID_NATIVE_CALLBACK;
        AccountCapabilitiesFetcherJni.get()
                .onCapabilitiesFetchComplete(accountCapabilities, mNativeCallback);
        mNativeCallback = INVALID_NATIVE_CALLBACK;
    }

    @NativeMethods
    interface Natives {
        void onCapabilitiesFetchComplete(
                AccountCapabilities accountCapabilities, long nativeCallback);
    }
}
