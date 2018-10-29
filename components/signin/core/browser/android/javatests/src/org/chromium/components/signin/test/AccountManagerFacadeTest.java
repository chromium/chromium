// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.accounts.Account;
import android.accounts.AuthenticatorDescription;
import android.app.Activity;
import android.content.Intent;
import android.support.annotation.Nullable;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.components.signin.AccountManagerDelegate;
import org.chromium.components.signin.AccountManagerDelegateException;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountsChangeObserver;

import java.util.concurrent.CountDownLatch;

/**
 * Tests for {@link AccountManagerFacade}. See also {@link AccountManagerFacadeRobolectricTest}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class AccountManagerFacadeTest {
    private BlockingAccountManagerDelegate mDelegate = new BlockingAccountManagerDelegate();
    // TODO(https://crbug.com/885235): Use Mockito instead when it no longer produces test errors.
    private static class BlockingAccountManagerDelegate implements AccountManagerDelegate {
        private final CountDownLatch mBlockGetAccounts = new CountDownLatch(1);

        // getAccountsSync always returns the same accounts, so there's no way to track observers.
        @Override
        public void registerObservers() {}
        @Override
        public void addObserver(AccountsChangeObserver observer) {}
        @Override
        public void removeObserver(AccountsChangeObserver observer) {}

        @Override
        public Account[] getAccountsSync() {
            // Block background thread that's trying to get accounts from the delegate.
            try {
                mBlockGetAccounts.await();
            } catch (InterruptedException e) {
                throw new RuntimeException(e);
            }
            return new Account[] {AccountManagerFacade.createAccountFromName("test@gmail.com")};
        }

        void unblockGetAccounts() {
            mBlockGetAccounts.countDown();
        }

        @Override
        public String getAuthToken(Account account, String authTokenScope) {
            throw new UnsupportedOperationException();
        }

        @Override
        public void invalidateAuthToken(String authToken) {
            throw new UnsupportedOperationException();
        }

        @Override
        public AuthenticatorDescription[] getAuthenticatorTypes() {
            throw new UnsupportedOperationException();
        }

        @Override
        public boolean hasFeatures(Account account, String[] features) {
            throw new UnsupportedOperationException();
        }

        @Override
        public void createAddAccountIntent(Callback<Intent> callback) {
            throw new UnsupportedOperationException();
        }

        @Override
        public void updateCredentials(
                Account account, Activity activity, @Nullable Callback<Boolean> callback) {
            throw new UnsupportedOperationException();
        }
    };

    @Before
    public void setUp() throws Exception {
        AccountManagerFacade.overrideAccountManagerFacadeForTests(mDelegate);
    }

    @Test
    @SmallTest
    public void testIsCachePopulated() throws AccountManagerDelegateException {
        // Cache shouldn't be populated until getAccountsSync is unblocked.
        assertFalse(AccountManagerFacade.get().isCachePopulated());

        mDelegate.unblockGetAccounts();
        // Wait for cache population to finish.
        AccountManagerFacade.get().getGoogleAccounts();
        assertTrue(AccountManagerFacade.get().isCachePopulated());
    }

    @Test
    @SmallTest
    public void testRunAfterCacheIsPopulated() throws InterruptedException {
        CountDownLatch firstCounter = new CountDownLatch(1);
        ThreadUtils.runOnUiThreadBlocking(() -> {
            // Add callback. This should be done on the main thread.
            AccountManagerFacade.get().runAfterCacheIsPopulated(firstCounter::countDown);
        });
        assertEquals("Callback shouldn't be invoked until cache is populated", 1,
                firstCounter.getCount());

        mDelegate.unblockGetAccounts();
        // Cache should be populated & callback should be invoked
        firstCounter.await();

        CountDownLatch secondCounter = new CountDownLatch(1);
        ThreadUtils.runOnUiThreadBlocking(() -> {
            AccountManagerFacade.get().runAfterCacheIsPopulated(secondCounter::countDown);
            assertEquals("Callback should be posted on UI thread, not executed synchronously", 1,
                    secondCounter.getCount());
        });
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        assertEquals(
                "Callback should be posted to UI thread right away", 0, secondCounter.getCount());
    }
}
