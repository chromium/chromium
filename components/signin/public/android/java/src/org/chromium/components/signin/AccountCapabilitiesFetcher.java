// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.base.JniOnceCallback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.signin.base.AccountCapabilities;
import org.chromium.components.signin.base.CoreAccountInfo;

/**
 * AccountCapabilitiesFetcher retrieves account capabilities from {@link AccountManagerFacade} and
 * passes them to native. It has a C++ counterpart located in
 * account_capabilities_fetcher_android.{h,cc}.
 */
@NullMarked
@JNINamespace("signin")
public class AccountCapabilitiesFetcher {
    private final CoreAccountInfo mCoreAccountInfo;
    private @Nullable JniOnceCallback<AccountCapabilities> mCallback;

    /** Constructs a fetcher tied to a single account identified by {@code coreAccountInfo}. */
    @CalledByNative
    public AccountCapabilitiesFetcher(
            @JniType("CoreAccountInfo") CoreAccountInfo coreAccountInfo,
            @JniType("base::OnceCallback<void(const base::android::JavaRef<jobject>&)>&&")
                    JniOnceCallback<AccountCapabilities> callback) {
        mCoreAccountInfo = coreAccountInfo;
        mCallback = callback;
    }

    @CalledByNative
    public void startFetchingAccountCapabilities() {
        var accountManagerFacade = AccountManagerFacadeProvider.getInstance();
        var capabilitiesPromise = accountManagerFacade.getAccountCapabilities(mCoreAccountInfo);
        if (capabilitiesPromise.isFulfilled()) {
            onCapabilitiesFetchComplete(capabilitiesPromise.getResult());
        } else {
            capabilitiesPromise.then(this::onCapabilitiesFetchComplete);
        }
    }

    @CalledByNative
    void destroy() {
        if (mCallback != null) {
            mCallback.destroy();
            mCallback = null;
        }
    }

    private void onCapabilitiesFetchComplete(AccountCapabilities accountCapabilities) {
        if (mCallback != null) {
            JniOnceCallback<AccountCapabilities> callback = mCallback;
            mCallback = null;
            callback.onResult(accountCapabilities);
        }
    }
}
