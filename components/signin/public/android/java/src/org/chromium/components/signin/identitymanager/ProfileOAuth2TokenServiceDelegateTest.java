// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.identitymanager;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import android.accounts.Account;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Spy;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.Promise;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.JniMocker;
import org.chromium.components.signin.AccessTokenData;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.AuthException;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;

import java.util.List;
import java.util.concurrent.CountDownLatch;

/** Tests for {@link ProfileOAuth2TokenServiceDelegate}. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class ProfileOAuth2TokenServiceDelegateTest {
    private static final long NATIVE_DELEGATE = 1000L;
    private static final String EMAIL = "test@gmail.com";
    private static final CoreAccountInfo CORE_ACCOUNT_INFO =
            CoreAccountInfo.createFromEmailAndGaiaId(
                    EMAIL, FakeAccountManagerFacade.toGaiaId(EMAIL));
    private static final Account ACCOUNT =
            AccountUtils.createAccountFromName(CORE_ACCOUNT_INFO.getEmail());

    /** Class handling GetAccessToken callbacks and providing a blocking {@link #getToken()}. */
    private static class CustomGetAccessTokenCallback
            implements ProfileOAuth2TokenServiceDelegate.GetAccessTokenCallback {
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

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Rule public final JniMocker mocker = new JniMocker();

    @Rule public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock private AccountTrackerService mAccountTrackerServiceMock;

    @Mock private ProfileOAuth2TokenServiceDelegate.Natives mNativeMock;

    private final CustomGetAccessTokenCallback mTokenCallback = new CustomGetAccessTokenCallback();

    @Spy
    private final FakeAccountManagerFacade mAccountManagerFacade = new FakeAccountManagerFacade();

    private ProfileOAuth2TokenServiceDelegate mDelegate;

    @Before
    public void setUp() {
        mocker.mock(ProfileOAuth2TokenServiceDelegateJni.TEST_HOOKS, mNativeMock);
        AccountManagerFacadeProvider.setInstanceForTests(mAccountManagerFacade);
        mDelegate =
                new ProfileOAuth2TokenServiceDelegate(NATIVE_DELEGATE, mAccountTrackerServiceMock);
    }

    @Test
    @SmallTest
    public void testGetOAuth2AccessTokenOnSuccess() throws AuthException {
        final String scope = "oauth2:http://example.com/scope";
        mAccountManagerFacade.addAccount(ACCOUNT);
        final AccessTokenData expectedToken =
                mAccountManagerFacade.getAccessToken(CORE_ACCOUNT_INFO, scope);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mDelegate.getAccessToken(CORE_ACCOUNT_INFO, scope, mTokenCallback);
                });
        Assert.assertEquals(expectedToken.getToken(), mTokenCallback.getToken());
    }

    @Test
    @SmallTest
    public void testGetOAuth2AccessTokenOnFailure() throws AuthException {
        final String scope = "oauth2:http://example.com/scope";
        mAccountManagerFacade.addAccount(ACCOUNT);
        doReturn(null)
                .when(mAccountManagerFacade)
                .getAccessToken(any(CoreAccountInfo.class), anyString());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mDelegate.getAccessToken(CORE_ACCOUNT_INFO, scope, mTokenCallback);
                });
        Assert.assertNull(mTokenCallback.getToken());
    }

    @Test
    @SmallTest
    public void testHasOAuth2RefreshTokenWhenAccountIsNotOnDevice() {
        mAccountManagerFacade.addAccount(ACCOUNT);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertFalse(mDelegate.hasOAuth2RefreshToken("test2@gmail.com"));
                });
    }

    @Test
    @SmallTest
    public void testHasOAuth2RefreshTokenWhenAccountIsOnDevice() {
        mAccountManagerFacade.addAccount(ACCOUNT);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertTrue(mDelegate.hasOAuth2RefreshToken(ACCOUNT.name));
                });
    }

    @Test
    @SmallTest
    public void testHasOAuth2RefreshTokenWhenCacheIsNotPopulated() {
        mAccountManagerFacade.addAccount(ACCOUNT);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    doReturn(new Promise<List<CoreAccountInfo>>())
                            .when(mAccountManagerFacade)
                            .getCoreAccountInfos();
                    Assert.assertFalse(mDelegate.hasOAuth2RefreshToken(ACCOUNT.name));
                });
    }

    @Test
    @SmallTest
    @Features.DisableFeatures(SigninFeatures.SEED_ACCOUNTS_REVAMP)
    public void testSeedAndReloadAccountsWhenAccountsAreSeeded() {
        mAccountManagerFacade.addAccount(ACCOUNT);
        doAnswer(
                        invocation -> {
                            Runnable runnable = invocation.getArgument(0);
                            runnable.run();
                            return null;
                        })
                .when(mAccountTrackerServiceMock)
                .legacySeedAccountsIfNeeded(any(Runnable.class));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mDelegate.legacySeedAndReloadAccountsWithPrimaryAccount(null);
                });
        verify(mNativeMock)
                .reloadAllAccountsWithPrimaryAccountAfterSeeding(
                        NATIVE_DELEGATE, null, new String[] {ACCOUNT.name});
    }

    @Test
    @SmallTest
    @Features.EnableFeatures(SigninFeatures.SEED_ACCOUNTS_REVAMP)
    public void testSeedAndReloadAccountsWhenAccountsAreSeeded_seedAccountRevampEnabled() {
        mAccountManagerFacade.addAccount(ACCOUNT);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mDelegate.seedAndReloadAccountsWithPrimaryAccount(
                            List.of(CORE_ACCOUNT_INFO), CORE_ACCOUNT_INFO.getId());
                });
        verify(mNativeMock)
                .seedAccountsThenReloadAllAccountsWithPrimaryAccount(
                        NATIVE_DELEGATE,
                        new CoreAccountInfo[] {CORE_ACCOUNT_INFO},
                        CORE_ACCOUNT_INFO.getId());
    }
}
