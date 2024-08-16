// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.components.signin;

import androidx.annotation.AnyThread;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.ServiceLoaderUtil;

/**
 * AccountManagerFacadeProvider is intended to group all the AccountManagerFacade instance
 * manipulation methods in one place.
 */
public class AccountManagerFacadeProvider {
    // Holder needed to avoid init on instrumentation thread in tests.
    private static class Holder {
        private static final AccountManagerFacade INSTANCE;

        static {
            AccountManagerDelegate delegate =
                    ServiceLoaderUtil.maybeCreate(AccountManagerDelegate.class);
            if (delegate == null) {
                delegate = new SystemAccountManagerDelegate();
            }
            INSTANCE = new AccountManagerFacadeImpl(delegate);
        }
    }

    private static AccountManagerFacade sInstanceForTesting;

    private AccountManagerFacadeProvider() {}

    /** Sets the test instance. */
    @VisibleForTesting
    @AnyThread
    public static void setInstanceForTests(AccountManagerFacade accountManagerFacade) {
        sInstanceForTesting = accountManagerFacade;
        ResettersForTesting.register(() -> sInstanceForTesting = null);
    }

    /**
     * Singleton instance getter.
     *
     * @return a singleton instance
     */
    @AnyThread
    @CalledByNative
    public static AccountManagerFacade getInstance() {
        if (sInstanceForTesting != null) {
            return sInstanceForTesting;
        }
        return Holder.INSTANCE;
    }
}
