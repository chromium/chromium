// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.identitymanager;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.metrics.SignoutReason;
import org.chromium.google_apis.gaia.CoreAccountId;

import java.util.List;

/**
 * IdentityMutator is the write interface of IdentityManager, see identity_mutator.h for more
 * information.
 */
@NullMarked
public class IdentityMutator {
    // Pointer to native IdentityMutator, not final because of destroy().
    private long mNativeIdentityMutator;

    @CalledByNative
    private IdentityMutator(long nativeIdentityMutator) {
        assert nativeIdentityMutator != 0;
        mNativeIdentityMutator = nativeIdentityMutator;
    }

    /** Called by native IdentityManager upon KeyedService's shutdown */
    @CalledByNative
    private void destroy() {
        mNativeIdentityMutator = 0;
    }

    /**
     * Marks the account with |accountId| as the primary account, and returns whether the operation
     * succeeded or not. To succeed, this requires that:
     *   - the account is known by the IdentityManager.
     *   - setting the primary account is allowed,
     *   - the account username is allowed by policy,
     *   - there is not already a primary account set.
     */
    public @PrimaryAccountError int setPrimaryAccount(
            CoreAccountId accountId,
            @ConsentLevel int consentLevel,
            @SigninAccessPoint int accessPoint,
            Runnable prefsSavedCallback) {
        return IdentityMutatorJni.get()
                .setPrimaryAccount(
                        mNativeIdentityMutator,
                        accountId,
                        consentLevel,
                        accessPoint,
                        prefsSavedCallback);
    }

    /**
     * Clears the primary account, revokes all consent, removes all accounts and returns whether the
     * operation succeeded .
     */
    public boolean clearPrimaryAccount(@SignoutReason int sourceMetric) {
        return IdentityMutatorJni.get().clearPrimaryAccount(mNativeIdentityMutator, sourceMetric);
    }

    /** Revokes sync consent for the primary account. */
    public void revokeSyncConsent(@SignoutReason int sourceMetric) {
        IdentityMutatorJni.get().revokeSyncConsent(mNativeIdentityMutator, sourceMetric);
    }

    /**
     * Seeds and reloads the given `accounts`. If `primaryAccountId` is not null then it must exist
     * in the given `accounts`.
     */
    public void seedAccountsThenReloadAllAccountsWithPrimaryAccount(
            List<AccountInfo> accounts, @Nullable CoreAccountId primaryAccountId) {
        IdentityMutatorJni.get()
                .seedAccountsThenReloadAllAccountsWithPrimaryAccount(
                        mNativeIdentityMutator,
                        accounts.toArray(new AccountInfo[0]),
                        primaryAccountId);
    }

    @NativeMethods
    interface Natives {
        @PrimaryAccountError
        int setPrimaryAccount(
                long nativeJniIdentityMutator,
                @JniType("CoreAccountId") CoreAccountId accountId,
                @ConsentLevel int consentLevel,
                @SigninAccessPoint int accessPoint,
                Runnable prefsSavedCallback);

        boolean clearPrimaryAccount(long nativeJniIdentityMutator, @SignoutReason int sourceMetric);

        void revokeSyncConsent(long nativeJniIdentityMutator, @SignoutReason int sourceMetric);

        void seedAccountsThenReloadAllAccountsWithPrimaryAccount(
                long nativeJniIdentityMutator,
                AccountInfo[] accounts,
                @Nullable CoreAccountId primaryAccountId);
    }
}
