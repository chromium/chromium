// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.identitymanager;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
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
@JNINamespace("signin")
public class IdentityMutator {
    // Pointer to native IdentityMutator, not final because of destroy().
    private long mNativeIdentityMutator;
    // TODO(crbug.com/572579970): Remove once refresh tokens loading state is moved to native
    // IdentityManager.
    private @Nullable IdentityManagerImpl mIdentityManager;

    @CalledByNative
    private IdentityMutator(long nativeIdentityMutator) {
        assert nativeIdentityMutator != 0;
        mNativeIdentityMutator = nativeIdentityMutator;
    }

    /**
     * Sets the IdentityManagerImpl instance.
     *
     * <p>TODO(crbug.com/572579970): Remove once refresh tokens loading state is moved to native
     * IdentityManager.
     */
    void setIdentityManager(IdentityManagerImpl identityManager) {
        mIdentityManager = identityManager;
    }

    /** Called by native IdentityManager upon KeyedService's shutdown */
    @CalledByNative
    private void destroy() {
        mNativeIdentityMutator = 0;
        mIdentityManager = null;
    }

    /**
     * Marks the account with |accountId| as the primary account, and returns whether the operation
     * succeeded or not. To succeed, this requires that:
     *
     * <ul>
     *   <li>the account is known by the IdentityManager.
     *   <li>setting the primary account is allowed,
     *   <li>the account username is allowed by policy,
     *   <li>there is not already a primary account set.
     * </ul>
     */
    public @PrimaryAccountError int setPrimaryAccount(
            CoreAccountId accountId,
            @SigninAccessPoint int accessPoint,
            Runnable prefsSavedCallback) {
        return IdentityMutatorJni.get()
                .setPrimaryAccount(
                        mNativeIdentityMutator, accountId, accessPoint, prefsSavedCallback);
    }

    /**
     * Marks the account with |accountId| as the primary account with sync consent, and returns
     * whether the operation succeeded or not. To succeed, this requires that:
     *
     * <ul>
     *   <li>the account is known by the IdentityManager.
     *   <li>setting the primary account is allowed,
     *   <li>the account username is allowed by policy,
     *   <li>there is not already a primary account set.
     * </ul>
     */
    public @PrimaryAccountError int setPrimaryAccountWithSyncConsentForTesting(
            CoreAccountId accountId,
            @SigninAccessPoint int accessPoint,
            Runnable prefsSavedCallback) {
        return IdentityMutatorJni.get()
                .setPrimaryAccountWithSyncConsentForTesting(
                        mNativeIdentityMutator, accountId, accessPoint, prefsSavedCallback);
    }

    // Removes the primary account and revokes the sync consent, but keep the
    // accounts signed in to the web and the tokens. Returns true if the action
    // was successful and false if there was no primary account set.
    public boolean removePrimaryAccountButKeepTokens(@SignoutReason int sourceMetric) {
        return IdentityMutatorJni.get()
                .removePrimaryAccountButKeepTokens(mNativeIdentityMutator, sourceMetric);
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

    /**
     * Marks refresh tokens loading as failed and notifies observers. Must only be called when
     * refresh tokens have not been loaded yet.
     *
     * <p>TODO(crbug.com/572579970): Remove once refresh tokens loading state is moved to native
     * IdentityManager.
     */
    public void setRefreshTokensLoadingFailed() {
        assert mIdentityManager != null;
        mIdentityManager.setRefreshTokensLoadingFailed();
    }

    @NativeMethods
    interface Natives {
        @PrimaryAccountError
        int setPrimaryAccount(
                long nativeJniIdentityMutator,
                @JniType("CoreAccountId") CoreAccountId accountId,
                @SigninAccessPoint int accessPoint,
                @JniType("base::OnceClosure") Runnable prefsSavedCallback);

        @PrimaryAccountError
        int setPrimaryAccountWithSyncConsentForTesting(
                long nativeJniIdentityMutator,
                @JniType("CoreAccountId") CoreAccountId accountId,
                @SigninAccessPoint int accessPoint,
                @JniType("base::OnceClosure") Runnable prefsSavedCallback);

        boolean removePrimaryAccountButKeepTokens(
                long nativeJniIdentityMutator, @SignoutReason int sourceMetric);

        void seedAccountsThenReloadAllAccountsWithPrimaryAccount(
                long nativeJniIdentityMutator,
                AccountInfo[] accounts,
                @Nullable CoreAccountId primaryAccountId);
    }
}
