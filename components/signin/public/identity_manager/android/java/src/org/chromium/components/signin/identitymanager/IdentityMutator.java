// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.identitymanager;

import androidx.annotation.Nullable;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
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
     * Marks the account with |account_id| as the primary account, and returns whether the operation
     * succeeded or not. To succeed, this requires that:
     *   - the account is known by the IdentityManager.
     *   - setting the primary account is allowed,
     *   - the account username is allowed by policy,
     *   - there is not already a primary account set.
     */
    public boolean setPrimaryAccount(CoreAccountId accountId) {
        return IdentityMutatorJni.get().setPrimaryAccount(mNativeIdentityMutator, accountId);
    }

    /**
     * Clears the primary account, and returns whether the operation succeeded or not. Depending on
     * |action|, the other accounts known to the IdentityManager may be deleted.
     */
    public boolean clearPrimaryAccount(@ClearAccountsAction int action,
            @SignoutReason int sourceMetric, @SignoutDelete int deleteMetric) {
        return IdentityMutatorJni.get().clearPrimaryAccount(
                mNativeIdentityMutator, action, sourceMetric, deleteMetric);
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
        public boolean setPrimaryAccount(long nativeJniIdentityMutator, CoreAccountId accountId);
        public boolean clearPrimaryAccount(long nativeJniIdentityMutator,
                @ClearAccountsAction int action, @SignoutReason int sourceMetric,
                @SignoutDelete int deleteMetric);
        public void reloadAllAccountsFromSystemWithPrimaryAccount(
                long nativeJniIdentityMutator, @Nullable CoreAccountId accountId);
    }
}
