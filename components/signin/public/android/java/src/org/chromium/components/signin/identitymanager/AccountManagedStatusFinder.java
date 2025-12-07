// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.identitymanager;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.signin.base.CoreAccountInfo;

import java.time.Duration;

/**
 * AccountManagedStatusFinder waits for the account managed status to be fetched. This is a Java
 * version of the C++ AccountManagedStatusFinder class.
 */
@NullMarked
public class AccountManagedStatusFinder {
    private final Callback<Integer> mAsyncCallback;

    private long mNativeAccountManagedStatusFinder;

    // This class needs to cache the outcome obtained from native, in case `getOutcome()` is invoked
    // after the native object is destroyed.
    private @AccountManagedStatusFinderOutcome int mCachedOutcome;

    /**
     * After an `AccountManagedStatusFinder` is instantiated, the account type may or may not be
     * known immediately. The `asyncCallback` will only be run if the account type was *not* known
     * immediately, i.e. if `GetOutcome()` was still `PENDING` when the constructor returned.
     */
    public AccountManagedStatusFinder(
            IdentityManager identityManager,
            CoreAccountInfo account,
            Callback<Integer> asyncCallback) {
        this(identityManager, account, asyncCallback, null);
    }

    /**
     * After an `AccountManagedStatusFinder` is instantiated, the account type may or may not be
     * known immediately. The `asyncCallback` will only be run if the account type was *not* known
     * immediately, i.e. if `GetOutcome()` was still `PENDING` when the constructor returned. If
     * `timeout` isn't null - the outcome will be determinted to `TIMEOUT` after the specified time
     * period.
     */
    public AccountManagedStatusFinder(
            IdentityManager identityManager,
            CoreAccountInfo account,
            Callback<Integer> asyncCallback,
            @Nullable Duration timeout) {
        assert identityManager != null && account != null && asyncCallback != null;
        mAsyncCallback = asyncCallback;

        // Negative timeout is used as "no timeout" value.
        long timeoutForNative = timeout != null ? timeout.toMillis() : -1;
        mNativeAccountManagedStatusFinder =
                AccountManagedStatusFinderJni.get()
                        .createNativeObject(
                                identityManager, account, this::onNativeResult, timeoutForNative);
        mCachedOutcome = getOutcomeFromNative();
        if (getOutcome() != AccountManagedStatusFinderOutcome.PENDING) {
            // The outcome is already known, it should be safe to destroy the native object.
            destroy();
        }
    }

    /**
     * Should be called after `AccountManagedStatusFinder` is no longer needed to release native
     * resources. Native resources are automatically released when the outcome is determined, so
     * calling this method is optional after the outcome has been determined.
     */
    public void destroy() {
        if (mNativeAccountManagedStatusFinder != 0) {
            mCachedOutcome = getOutcomeFromNative();
            AccountManagedStatusFinderJni.get()
                    .destroyNativeObject(mNativeAccountManagedStatusFinder);
            mNativeAccountManagedStatusFinder = 0;
        }
    }

    /** Returns the managed status for the account or `PENDING` if it is not known yet. */
    public @AccountManagedStatusFinderOutcome int getOutcome() {
        return mCachedOutcome;
    }

    private @AccountManagedStatusFinderOutcome int getOutcomeFromNative() {
        return AccountManagedStatusFinderJni.get()
                .getOutcomeFromNativeObject(mNativeAccountManagedStatusFinder);
    }

    private void onNativeResult() {
        mCachedOutcome = getOutcomeFromNative();
        // The native callback can be invoked only once, destroy the native object.
        destroy();

        mAsyncCallback.onResult(getOutcome());
    }

    @NativeMethods
    public interface Natives {
        long createNativeObject(
                @JniType("IdentityManager*") IdentityManager identityManager,
                @JniType("CoreAccountInfo") CoreAccountInfo account,
                @JniType("base::RepeatingClosure") Runnable asyncCallback,
                long timeout);

        void destroyNativeObject(long nativeAccountManagedStatusFinder);

        int getOutcomeFromNativeObject(long nativeAccountManagedStatusFinder);
    }
}
