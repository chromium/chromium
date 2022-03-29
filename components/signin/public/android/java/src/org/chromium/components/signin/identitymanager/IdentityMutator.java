// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.identitymanager;

import androidx.annotation.Nullable;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.signin.base.CoreAccountId;
import org.chromium.components.signin.metrics.SignoutDelete;
import org.chromium.components.signin.metrics.SignoutReason;

/**
 * IdentityMutator is the write interface of IdentityManager, see identity_mutator.h for more
 * information.
 */
public class IdentityMutator {
    private static final String TAG = "IdentityMutator";

    // Pointer to native IdentityMutator, not final because of destroy().
    private long mNativeIdentityMutator;

    @CalledByNative
    private IdentityMutator(long nativeIdentityMutator) {
        assert nativeIdentityMutator != 0;
        mNativeIdentityMutator = nativeIdentityMutator;
    }

    /**
     * Called by native IdentityManager upon KeyedService's shutdown
     */
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
    public boolean setPrimaryAccount(CoreAccountId accountId, @ConsentLevel int consentLevel) {
        return IdentityMutatorJni.get().setPrimaryAccount(
                mNativeIdentityMutator, accountId, consentLevel);
    }

    /**
     * Clears the primary account, revokes all consent, removes all accounts and returns whether the
     * operation succeeded .
     */
    public boolean clearPrimaryAccount(
            @SignoutReason int sourceMetric, @SignoutDelete int deleteMetric) {
        return IdentityMutatorJni.get().clearPrimaryAccount(
                mNativeIdentityMutator, sourceMetric, deleteMetric);
    }

    /**
     * Revokes sync consent for the primary account.
     *
     * If the transition from Sync to Signin consent level is not supported for this user, then this
     * method will also clear the primary account.
     * TODO(crbug.com/1306031): remove the sentence above once ALLOW_SYNC_OFF_FOR_CHILD_ACCOUNTS is
     * cleaned up.
     */
    public void revokeSyncConsent(
            @SignoutReason int sourceMetric, @SignoutDelete int deleteMetric) {
        IdentityMutatorJni.get().revokeSyncConsent(
                mNativeIdentityMutator, sourceMetric, deleteMetric);
    }

    /**
     * Reloads the accounts in the token service from the system accounts. This API calls
     * ProfileOAuth2TokenServiceDelegate::ReloadAllAccountsFromSystemWithPrimaryAccount.
     */
    public void reloadAllAccountsFromSystemWithPrimaryAccount(@Nullable CoreAccountId accountId) {
        IdentityMutatorJni.get().reloadAllAccountsFromSystemWithPrimaryAccount(
                mNativeIdentityMutator, accountId);
    }

    @NativeMethods
    interface Natives {
        public boolean setPrimaryAccount(long nativeJniIdentityMutator, CoreAccountId accountId,
                @ConsentLevel int consentLevel);
        public boolean clearPrimaryAccount(long nativeJniIdentityMutator,
                @SignoutReason int sourceMetric, @SignoutDelete int deleteMetric);
        public void revokeSyncConsent(long nativeJniIdentityMutator,
                @SignoutReason int sourceMetric, @SignoutDelete int deleteMetric);
        public void reloadAllAccountsFromSystemWithPrimaryAccount(
                long nativeJniIdentityMutator, @Nullable CoreAccountId accountId);
    }
}
