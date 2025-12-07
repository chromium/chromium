// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.accounts.Account;

import androidx.annotation.Nullable;
import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.FeatureOverrides;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.params.BaseJUnit4RunnerDelegate;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.test.util.FakeAccountManagerDelegate;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.google_apis.gaia.GoogleServiceAuthError;

import java.util.Arrays;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/** Tests for {@link AccountManagerFacade}. See also {@link AccountManagerFacadeImplTest}. */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(BaseJUnit4RunnerDelegate.class)
@DoNotBatch(reason = "Test needs to be torn down to allow parameters to take effect")
public class AccountManagerFacadeTest {
    private static final class BlockingAccountManagerDelegate extends FakeAccountManagerDelegate {
        private @Nullable CountDownLatch mGetAccountsLatch;
        private @Nullable CountDownLatch mGetAuthTokenLatch;

        @Override
        public Account[] getAccountsSynchronous() throws AccountManagerDelegateException {
            maybeBlockOnLatch(mGetAccountsLatch);
            return super.getAccountsSynchronous();
        }

        @Override
        public List<PlatformAccount> getPlatformAccountsSynchronous()
                throws AccountManagerDelegateException {
            maybeBlockOnLatch(mGetAccountsLatch);
            return super.getPlatformAccountsSynchronous();
        }

        void blockGetAccount() {
            assertThat(mGetAccountsLatch).isNull();
            mGetAccountsLatch = new CountDownLatch(1);
        }

        void unblockGetAccounts() {
            mGetAccountsLatch.countDown();
        }

        @Override
        public AccessTokenData getAccessToken(Account account, String scope) throws AuthException {
            maybeBlockOnLatch(mGetAuthTokenLatch);
            return super.getAccessToken(account, scope);
        }

        @Override
        public AccessTokenData getAccessTokenForPlatformAccount(
                PlatformAccount platformAccount, String authTokenScopes) throws AuthException {
            maybeBlockOnLatch(mGetAuthTokenLatch);
            return super.getAccessTokenForPlatformAccount(platformAccount, authTokenScopes);
        }

        void blockGetAuthToken() {
            assertThat(mGetAuthTokenLatch).isNull();
            mGetAuthTokenLatch = new CountDownLatch(1);
        }

        void unblockGetAuthToken() {
            mGetAuthTokenLatch.countDown();
        }

        private void maybeBlockOnLatch(@Nullable CountDownLatch latch) {
            if (latch == null) {
                // The method is not blocked,
                return;
            }
            try {
                if (!latch.await(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL, TimeUnit.MILLISECONDS)) {
                    throw new RuntimeException("Timed out waiting for a blocked call!");
                }
            } catch (InterruptedException e) {
                throw new RuntimeException(e);
            }
        }
    }

    /** Class handling GetAccessToken callbacks and providing a blocking {@link #getToken()}. */
    private static class CustomGetAccessTokenCallback
            implements AccountManagerFacade.GetAccessTokenCallback {
        private String mToken;
        private final CountDownLatch mTokenRetrievedCountDown = new CountDownLatch(1);

        /**
         * Blocks until the callback is called once and returns the token. See {@link
         * CountDownLatch#await}
         */
        String getToken() {
            try {
                mTokenRetrievedCountDown.await();
            } catch (InterruptedException e) {
                throw new RuntimeException("Interrupted or timed-out while waiting for updates", e);
            }
            return mToken;
        }

        boolean isReady() {
            return mTokenRetrievedCountDown.getCount() == 0;
        }

        @Override
        public void onGetTokenSuccess(AccessTokenData token) {
            mToken = token.getToken();
            mTokenRetrievedCountDown.countDown();
        }

        @Override
        public void onGetTokenFailure(GoogleServiceAuthError authError) {
            mToken = null;
            mTokenRetrievedCountDown.countDown();
        }
    }

    private static final String TOKEN_SCOPE = "http://example.com/scope";
    private static final String OAUTH2_SCOPE_PREFIX = "oauth2:";

    public static class AccountManagerFacadeTestParams implements ParameterProvider {
        private static final List<ParameterSet> sAccountManagerFacadeTestParams =
                Arrays.asList(
                        new ParameterSet().value(true).name("migrateAccountManagerDelegateEnabled"),
                        new ParameterSet()
                                .value(false)
                                .name("migrateAccountManagerDelegateDisabled"));

        @Override
        public List<ParameterSet> getParameters() {
            return sAccountManagerFacadeTestParams;
        }
    }

    @ParameterAnnotations.UseMethodParameterBefore(AccountManagerFacadeTestParams.class)
    public void enableMigrateAccountManagerDelegateFlag(boolean enabled) {
        FeatureOverrides.overrideFlag(SigninFeatures.MIGRATE_ACCOUNT_MANAGER_DELEGATE, enabled);
    }

    @Before
    public void setUp() {
        // Load native library because GoogleServiceAuthError may be created during access token
        // tests and it calls into native code.
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
    }

    @Test
    @SmallTest
    @ParameterAnnotations.UseMethodParameter(AccountManagerFacadeTestParams.class)
    public void testIsCachePopulated(boolean isMigrationEnabled) throws InterruptedException {
        BlockingAccountManagerDelegate blockingDelegate = new BlockingAccountManagerDelegate();
        blockingDelegate.blockGetAccount();
        AccountManagerFacade facade =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> new AccountManagerFacadeImpl(blockingDelegate));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Cache shouldn't be populated until getAccountsSync is unblocked.
                    assertFalse(facade.getAccounts().isFulfilled());
                });

        blockingDelegate.unblockGetAccounts();
        CountDownLatch countDownLatch = new CountDownLatch(1);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    facade.getAccounts()
                            .then(
                                    accounts -> {
                                        countDownLatch.countDown();
                                    });
                });
        // Wait for cache population to finish.
        countDownLatch.await();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertTrue(facade.getAccounts().isFulfilled());
                });
    }

    @Test
    @SmallTest
    @ParameterAnnotations.UseMethodParameter(AccountManagerFacadeTestParams.class)
    public void testRunAfterCacheIsPopulated(boolean isMigrationEnabled)
            throws InterruptedException {
        BlockingAccountManagerDelegate blockingDelegate = new BlockingAccountManagerDelegate();
        blockingDelegate.blockGetAccount();
        AccountManagerFacade facade =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> new AccountManagerFacadeImpl(blockingDelegate));

        CountDownLatch firstCounter = new CountDownLatch(1);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Add callback. This should be done on the main thread.
                    facade.getAccounts()
                            .then(
                                    accounts -> {
                                        firstCounter.countDown();
                                    });
                });
        assertEquals(
                "Callback shouldn't be invoked until cache is populated",
                1,
                firstCounter.getCount());

        blockingDelegate.unblockGetAccounts();
        // Cache should be populated & callback should be invoked
        firstCounter.await();

        CountDownLatch secondCounter = new CountDownLatch(1);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    facade.getAccounts()
                            .then(
                                    accounts -> {
                                        secondCounter.countDown();
                                    });
                    assertEquals(
                            "Callback should be posted on UI thread, not "
                                    + "executed synchronously",
                            1,
                            secondCounter.getCount());
                });
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        assertEquals(
                "Callback should be posted to UI thread right away", 0, secondCounter.getCount());
    }

    @Test
    @SmallTest
    @ParameterAnnotations.UseMethodParameter(AccountManagerFacadeTestParams.class)
    public void testGetOAuth2AccessTokenOnSuccess(boolean isMigrationEnabled) throws AuthException {
        FakeAccountManagerDelegate delegate = new FakeAccountManagerDelegate();
        AccountManagerFacade facade =
                ThreadUtils.runOnUiThreadBlocking(() -> new AccountManagerFacadeImpl(delegate));
        PlatformAccount platformAccount = delegate.addAccount(TestAccounts.ACCOUNT1);
        waitForAccountToBeAdded(facade, TestAccounts.ACCOUNT1);

        final AccessTokenData expectedToken;
        if (isMigrationEnabled) {
            assert SigninFeatureMap.sMigrateAccountManagerDelegate.isEnabled();
            expectedToken = delegate.getAccessTokenForPlatformAccount(platformAccount, TOKEN_SCOPE);
            assertNotNull(expectedToken);
        } else {
            expectedToken =
                    delegate.getAccessToken(
                            CoreAccountInfo.getAndroidAccountFrom(TestAccounts.ACCOUNT1),
                            OAUTH2_SCOPE_PREFIX + TOKEN_SCOPE);
            assertNotNull(expectedToken);
        }

        CustomGetAccessTokenCallback callback = new CustomGetAccessTokenCallback();
        ThreadUtils.runOnUiThread(
                () -> facade.getAccessToken(TestAccounts.ACCOUNT1, TOKEN_SCOPE, callback));
        assertEquals(expectedToken.getToken(), callback.getToken());
    }

    @Test
    @SmallTest
    @ParameterAnnotations.UseMethodParameter(AccountManagerFacadeTestParams.class)
    public void testGetOAuth2AccessTokenOnFailure(boolean isMigrationEnabled) throws AuthException {
        FakeAccountManagerDelegate delegate = new FakeAccountManagerDelegate();
        AccountManagerFacade facade =
                ThreadUtils.runOnUiThreadBlocking(() -> new AccountManagerFacadeImpl(delegate));

        CustomGetAccessTokenCallback callback = new CustomGetAccessTokenCallback();
        // Request a token for an account that wasn't added.
        ThreadUtils.runOnUiThread(
                () -> facade.getAccessToken(TestAccounts.ACCOUNT1, TOKEN_SCOPE, callback));

        assertNull(callback.getToken());
    }

    @Test
    @SmallTest
    @ParameterAnnotations.UseMethodParameter(AccountManagerFacadeTestParams.class)
    public void testGetAndInvalidateAccessToken(boolean isMigrationEnabled) throws Exception {
        FakeAccountManagerDelegate delegate = new FakeAccountManagerDelegate();
        AccountManagerFacade facade =
                ThreadUtils.runOnUiThreadBlocking(() -> new AccountManagerFacadeImpl(delegate));
        delegate.addAccount(TestAccounts.ACCOUNT1);
        waitForAccountToBeAdded(facade, TestAccounts.ACCOUNT1);

        CustomGetAccessTokenCallback callback1 = new CustomGetAccessTokenCallback();
        ThreadUtils.runOnUiThread(
                () -> facade.getAccessToken(TestAccounts.ACCOUNT1, TOKEN_SCOPE, callback1));
        String originalToken = callback1.getToken();

        CustomGetAccessTokenCallback callback2 = new CustomGetAccessTokenCallback();
        ThreadUtils.runOnUiThread(
                () -> facade.getAccessToken(TestAccounts.ACCOUNT1, TOKEN_SCOPE, callback2));
        assertEquals(
                "The same token should be returned before invalidating the token.",
                callback2.getToken(),
                originalToken);

        CallbackHelper invalidationCallback = new CallbackHelper();
        ThreadUtils.runOnUiThread(
                () ->
                        facade.invalidateAccessToken(
                                originalToken, () -> invalidationCallback.notifyCalled()));
        invalidationCallback.waitForOnly();

        CustomGetAccessTokenCallback callback3 = new CustomGetAccessTokenCallback();
        ThreadUtils.runOnUiThread(
                () -> facade.getAccessToken(TestAccounts.ACCOUNT1, TOKEN_SCOPE, callback3));
        assertNotEquals(
                "A different token should be returned since the original token is invalidated.",
                callback3.getToken(),
                originalToken);
        assertNotNull(callback3.getToken());
    }

    @Test
    @SmallTest
    public void testWaitForPendingTokenRequestsToComplete() throws Exception {
        BlockingAccountManagerDelegate blockingDelegate = new BlockingAccountManagerDelegate();
        blockingDelegate.addAccount(TestAccounts.ACCOUNT1);
        blockingDelegate.blockGetAuthToken();
        AccountManagerFacade facade =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> new AccountManagerFacadeImpl(blockingDelegate));

        CustomGetAccessTokenCallback tokenCallback = new CustomGetAccessTokenCallback();
        ThreadUtils.runOnUiThreadBlocking(
                () -> facade.getAccessToken(TestAccounts.ACCOUNT1, TOKEN_SCOPE, tokenCallback));

        CallbackHelper pendingRequestsCompleteCallback = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        facade.waitForPendingTokenRequestsToComplete(
                                () -> pendingRequestsCompleteCallback.notifyCalled()));

        // Since getAccessToken is blocked, neither of the callbacks should be invoked.
        assertFalse(tokenCallback.isReady());
        assertEquals(0, pendingRequestsCompleteCallback.getCallCount());

        // Unblock the delegate and wait for callbacks to be invoked.
        blockingDelegate.unblockGetAuthToken();
        pendingRequestsCompleteCallback.waitForOnly();
        assertTrue(tokenCallback.isReady());
    }

    @Test
    @SmallTest
    public void testFetchAccessTokenIfNoAccountsAreLoaded() throws Exception {
        FeatureOverrides.overrideFlag(SigninFeatures.MIGRATE_ACCOUNT_MANAGER_DELEGATE, true);

        FakeAccountManagerDelegate delegate = new FakeAccountManagerDelegate();
        AccountManagerFacade facade =
                ThreadUtils.runOnUiThreadBlocking(() -> new AccountManagerFacadeImpl(delegate));
        assert SigninFeatureMap.sMigrateAccountManagerDelegate.isEnabled();
        CustomGetAccessTokenCallback callback = new CustomGetAccessTokenCallback();

        // Fetching account with a test
        ThreadUtils.runOnUiThread(
                () -> facade.getAccessToken(TestAccounts.ACCOUNT1, TOKEN_SCOPE, callback));

        assertNull(callback.getToken());
    }

    private static void waitForAccountToBeAdded(AccountManagerFacade facade, AccountInfo account) {
        CriteriaHelper.pollUiThread(
                () -> {
                    return facade.getAccounts().isFulfilled()
                            && facade.getAccounts().getResult().contains(account);
                });
    }
}
