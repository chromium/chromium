// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.identitymanager;

import android.accounts.Account;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.components.signin.AccessTokenData;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;

import java.util.Arrays;
import java.util.concurrent.CountDownLatch;

/** Tests for {@link ProfileOAuth2TokenServiceDelegate}. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class ProfileOAuth2TokenServiceDelegateTest {
    private FakeAccountManagerFacade mAccountManagerFacade;

    private ProfileOAuth2TokenServiceDelegate mProfileOAuth2TokenServiceDelegate;

    /**
     * Class handling GetAccessToken callbacks and providing a blocking {@link
     * #getToken()}.
     */
    private static class GetAccessTokenCallbackForTest
            implements ProfileOAuth2TokenServiceDelegate.GetAccessTokenCallback {
        private String mToken;
        final CountDownLatch mTokenRetrievedCountDown = new CountDownLatch(1);

        /**
         * Blocks until the callback is called once and returns the token.
         * See {@link CountDownLatch#await}
         */
        public String getToken() {
            try {
                mTokenRetrievedCountDown.await();
            } catch (InterruptedException e) {
                throw new RuntimeException("Interrupted or timed-out while waiting for updates", e);
            }
            return mToken;
        }

        @Override
        public void onGetTokenSuccess(AccessTokenData token) {
            mToken = token.getToken();
            mTokenRetrievedCountDown.countDown();
        }

        @Override
        public void onGetTokenFailure(boolean isTransientError) {
            mToken = null;
            mTokenRetrievedCountDown.countDown();
        }
    }

    @Before
    public void setUp() {
        mAccountManagerFacade = new FakeAccountManagerFacade(null);
        mProfileOAuth2TokenServiceDelegate = new ProfileOAuth2TokenServiceDelegate(
                0 /*nativeProfileOAuth2TokenServiceDelegateDelegate*/,
                null /* AccountTrackerService */, mAccountManagerFacade);
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testGetAccountsNoAccountsRegistered() {
        String[] sysAccounts = mProfileOAuth2TokenServiceDelegate.getSystemAccountNames();
        Assert.assertEquals("There should be no accounts registered", 0, sysAccounts.length);
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testGetAccountsOneAccountRegistered() {
        Account account1 = AccountUtils.createAccountFromName("foo@gmail.com");
        mAccountManagerFacade.addAccount(account1);

        String[] sysAccounts = mProfileOAuth2TokenServiceDelegate.getSystemAccountNames();
        Assert.assertEquals("There should be one registered account", 1, sysAccounts.length);
        Assert.assertEquals("The account should be " + account1, account1.name, sysAccounts[0]);
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testGetAccountsTwoAccountsRegistered() {
        Account account1 = AccountUtils.createAccountFromName("foo@gmail.com");
        mAccountManagerFacade.addAccount(account1);
        Account account2 = AccountUtils.createAccountFromName("bar@gmail.com");
        mAccountManagerFacade.addAccount(account2);

        String[] sysAccounts = mProfileOAuth2TokenServiceDelegate.getSystemAccountNames();
        Assert.assertEquals("There should be two registered account", 2, sysAccounts.length);
        Assert.assertTrue("The list should contain " + account1,
                Arrays.asList(sysAccounts).contains(account1.name));
        Assert.assertTrue("The list should contain " + account2,
                Arrays.asList(sysAccounts).contains(account2.name));
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testGetOAuth2AccessTokenWithTimeoutOnSuccess() {
        String authToken = "someToken";
        // Auth token should be successfully received.
        runTestOfGetOAuth2AccessTokenWithTimeout(authToken);
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testGetOAuth2AccessTokenWithTimeoutOnError() {
        String authToken = null;
        // Should not crash when auth token is null.
        runTestOfGetOAuth2AccessTokenWithTimeout(authToken);
    }

    private void runTestOfGetOAuth2AccessTokenWithTimeout(String expectedToken) {
        String scope = "oauth2:http://example.com/scope";
        Account account = AccountUtils.createAccountFromName("test@gmail.com");
        mAccountManagerFacade.addAccount(account);
        GetAccessTokenCallbackForTest callback = new GetAccessTokenCallbackForTest();

        ThreadUtils.runOnUiThreadBlocking(() -> {
            mProfileOAuth2TokenServiceDelegate.getAccessToken(account, scope, callback);
        });

        Assert.assertEquals(mAccountManagerFacade.getAccessToken(account, scope).getToken(),
                callback.getToken());
    }

    @Test
    @SmallTest
    public void testHasOAuth2RefreshTokenWhenAccountIsNotOnDevice() {
        mAccountManagerFacade.addAccount(AccountUtils.createAccountFromName("test1@gmail.com"));
        Assert.assertFalse(
                mProfileOAuth2TokenServiceDelegate.hasOAuth2RefreshToken("test2@gmail.com"));
    }

    @Test
    @SmallTest
    public void testHasOAuth2RefreshTokenWhenAccountIsOnDevice() {
        final String accountEmail = "test1@gmail.com";
        mAccountManagerFacade.addAccount(AccountUtils.createAccountFromName(accountEmail));
        Assert.assertTrue(mProfileOAuth2TokenServiceDelegate.hasOAuth2RefreshToken(accountEmail));
    }
}
