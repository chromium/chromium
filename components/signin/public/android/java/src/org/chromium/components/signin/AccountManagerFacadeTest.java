// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin;

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

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.test.util.FakeAccountManagerDelegate;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/** Tests for {@link AccountManagerFacade}. See also {@link AccountManagerFacadeImplTest}. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class AccountManagerFacadeTest {
    private static final class BlockingAccountManagerDelegate extends FakeAccountManagerDelegate {
        private @Nullable CountDownLatch mGetAccountsLatch;
        private @Nullable CountDownLatch mGetAuthTokenLatch;

        @Override
        public Account[] getAccountsSynchronous() throws AccountManagerDelegateException {
            maybeBlockOnLatch(mGetAccountsLatch);
            return super.getAccountsSynchronous();
        }

        void blockGetAccount() {
            assert mGetAccountsLatch == null;
            mGetAccountsLatch = new CountDownLatch(1);
        }

        void unblockGetAccounts() {
            mGetAccountsLatch.countDown();
        }

        @Override
        public AccessTokenData getAuthToken(Account account, String scope) throws AuthException {
            maybeBlockOnLatch(mGetAuthTokenLatch);
            return super.getAuthToken(account, scope);
        }

        void blockGetAuthToken() {
            assert mGetAuthTokenLatch == null;
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
        public void onGetTokenFailure(boolean isTransientError) {
            mToken = null;
            mTokenRetrievedCountDown.countDown();
        }
    }

    private static final AccountInfo ACCOUNT =
            new AccountInfo.Builder(
                            "test@gmail.com", FakeAccountManagerFacade.toGaiaId("test@gmail.com"))
                    .build();
    private static final String TOKEN_SCOPE = "oauth2:http://example.com/scope";

    @Test
    @SmallTest
    public void testIsCachePopulated() throws InterruptedException {
        BlockingAccountManagerDelegate blockingDelegate = new BlockingAccountManagerDelegate();
        blockingDelegate.blockGetAccount();
        AccountManagerFacade facade =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> new AccountManagerFacadeImpl(blockingDelegate));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Cache shouldn't be populated until getAccountsSync is unblocked.
                    assertFalse(facade.getCoreAccountInfos().isFulfilled());
                });

        blockingDelegate.unblockGetAccounts();
        CountDownLatch countDownLatch = new CountDownLatch(1);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    facade.getCoreAccountInfos()
                            .then(
                                    coreAccountInfos -> {
                                        countDownLatch.countDown();
                                    });
                });
        // Wait for cache population to finish.
        countDownLatch.await();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertTrue(facade.getCoreAccountInfos().isFulfilled());
                });
    }

    @Test
    @SmallTest
    public void testRunAfterCacheIsPopulated() throws InterruptedException {
        BlockingAccountManagerDelegate blockingDelegate = new BlockingAccountManagerDelegate();
        blockingDelegate.blockGetAccount();
        AccountManagerFacade facade =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> new AccountManagerFacadeImpl(blockingDelegate));

        CountDownLatch firstCounter = new CountDownLatch(1);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Add callback. This should be done on the main thread.
                    facade.getCoreAccountInfos()
                            .then(
                                    coreAccountInfos -> {
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
                    facade.getCoreAccountInfos()
                            .then(
                                    coreAccountInfos -> {
                                        secondCounter.countDown();
                                    });
                    assertEquals(
                            "Callback should be posted on UI thread, not executed synchronously",
                            1,
                            secondCounter.getCount());
                });
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        assertEquals(
                "Callback should be posted to UI thread right away", 0, secondCounter.getCount());
    }

    @Test
    @SmallTest
    public void testGetOAuth2AccessTokenOnSuccess() throws AuthException {
        FakeAccountManagerDelegate delegate = new FakeAccountManagerDelegate();
        AccountManagerFacade facade =
                ThreadUtils.runOnUiThreadBlocking(() -> new AccountManagerFacadeImpl(delegate));

        delegate.addAccount(ACCOUNT);
        AccessTokenData expectedToken =
                delegate.getAuthToken(CoreAccountInfo.getAndroidAccountFrom(ACCOUNT), TOKEN_SCOPE);

        CustomGetAccessTokenCallback callback = new CustomGetAccessTokenCallback();
        ThreadUtils.runOnUiThread(() -> facade.getAccessToken(ACCOUNT, TOKEN_SCOPE, callback));
        assertEquals(expectedToken.getToken(), callback.getToken());
    }

    @Test
    @SmallTest
    public void testGetOAuth2AccessTokenOnFailure() throws AuthException {
        FakeAccountManagerDelegate delegate = new FakeAccountManagerDelegate();
        AccountManagerFacade facade =
                ThreadUtils.runOnUiThreadBlocking(() -> new AccountManagerFacadeImpl(delegate));

        CustomGetAccessTokenCallback callback = new CustomGetAccessTokenCallback();
        // Request a token for an account that wasn't added.
        ThreadUtils.runOnUiThread(() -> facade.getAccessToken(ACCOUNT, TOKEN_SCOPE, callback));

        assertNull(callback.getToken());
    }

    @Test
    @SmallTest
    public void testGetAndInvalidateAccessToken() throws Exception {
        FakeAccountManagerDelegate delegate = new FakeAccountManagerDelegate();
        AccountManagerFacade facade =
                ThreadUtils.runOnUiThreadBlocking(() -> new AccountManagerFacadeImpl(delegate));

        delegate.addAccount(ACCOUNT);

        CustomGetAccessTokenCallback callback1 = new CustomGetAccessTokenCallback();
        ThreadUtils.runOnUiThread(() -> facade.getAccessToken(ACCOUNT, TOKEN_SCOPE, callback1));
        String originalToken = callback1.getToken();

        CustomGetAccessTokenCallback callback2 = new CustomGetAccessTokenCallback();
        ThreadUtils.runOnUiThread(() -> facade.getAccessToken(ACCOUNT, TOKEN_SCOPE, callback2));
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
        ThreadUtils.runOnUiThread(() -> facade.getAccessToken(ACCOUNT, TOKEN_SCOPE, callback3));
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
        blockingDelegate.addAccount(ACCOUNT);
        blockingDelegate.blockGetAuthToken();
        AccountManagerFacade facade =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> new AccountManagerFacadeImpl(blockingDelegate));

        CustomGetAccessTokenCallback tokenCallback = new CustomGetAccessTokenCallback();
        ThreadUtils.runOnUiThreadBlocking(
                () -> facade.getAccessToken(ACCOUNT, TOKEN_SCOPE, tokenCallback));

        CallbackHelper pendingRequestsCompleteCallback = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        facade.waitForPendingTokenRequestsToComplete(
                                () -> pendingRequestsCompleteCallback.notifyCalled()));

        // Since getAccessToken is blocked, neither of the callbacks should be invoked.
        assertFalse(tokenCallback.isReady());
        assertEquals(pendingRequestsCompleteCallback.getCallCount(), 0);

        // Unblock the delegate and wait for callbacks to be invoked.
        blockingDelegate.unblockGetAuthToken();
        pendingRequestsCompleteCallback.waitForOnly();
        assertTrue(tokenCallback.isReady());
    }
}
