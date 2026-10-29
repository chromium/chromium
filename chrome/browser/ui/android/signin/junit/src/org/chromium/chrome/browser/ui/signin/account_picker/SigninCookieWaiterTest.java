// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.account_picker;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtilsJni;
import org.chromium.chrome.browser.signin.services.WebSigninTrackerBridge;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.browser.WebSigninTrackerResult;
import org.chromium.components.signin.metrics.AccountConsistencyPromoAction;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.test.util.TestAccounts;

import java.util.concurrent.atomic.AtomicReference;

/** Tests for {@link SigninCookieWaiter}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SigninCookieWaiterTest {
    private static final @SigninAccessPoint int ACCESS_POINT = SigninAccessPoint.EXTENSIONS;

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private WebSigninTrackerBridge.Factory mWebSigninTrackerBridgeFactoryMock;
    @Mock private WebSigninTrackerBridge mWebSigninTrackerBridgeMock;
    @Mock private Profile mProfileMock;
    @Mock private SigninMetricsUtils.Natives mSigninMetricsUtilsJniMock;
    @Mock private Callback<@PostSigninOperationResult Integer> mOnCompleteMock;

    @Captor
    private ArgumentCaptor<Callback<@WebSigninTrackerResult Integer>> mWebSigninCallbackCaptor;

    private SigninCookieWaiter mWaiter;

    @Before
    public void setUp() {
        SigninMetricsUtilsJni.setInstanceForTesting(mSigninMetricsUtilsJniMock);
        lenient()
                .when(
                        mWebSigninTrackerBridgeFactoryMock.createWithCoreAccountId(
                                eq(mProfileMock), any(), any()))
                .thenReturn(mWebSigninTrackerBridgeMock);
        mWaiter =
                new SigninCookieWaiter(
                        mProfileMock, mWebSigninTrackerBridgeFactoryMock, ACCESS_POINT);
    }

    @Test
    public void testSuccess() {
        mWaiter.waitForCookies(TestAccounts.ACCOUNT1.getId(), mOnCompleteMock);

        triggerWebSigninCallback(WebSigninTrackerResult.SUCCESS);

        verify(mOnCompleteMock).onResult(PostSigninOperationResult.SUCCESS);
        verifyNoInteractions(mSigninMetricsUtilsJniMock);
    }

    @Test
    public void testAuthError() {
        mWaiter.waitForCookies(TestAccounts.ACCOUNT1.getId(), mOnCompleteMock);

        triggerWebSigninCallback(WebSigninTrackerResult.AUTH_ERROR);

        verify(mOnCompleteMock).onResult(PostSigninOperationResult.AUTH_ERROR);
        verify(mSigninMetricsUtilsJniMock)
                .logAccountConsistencyPromoAction(
                        AccountConsistencyPromoAction.AUTH_ERROR_SHOWN, ACCESS_POINT);
    }

    @Test
    public void testOtherError() {
        mWaiter.waitForCookies(TestAccounts.ACCOUNT1.getId(), mOnCompleteMock);

        triggerWebSigninCallback(WebSigninTrackerResult.OTHER_ERROR);

        verify(mOnCompleteMock).onResult(PostSigninOperationResult.OTHER_ERROR);
        verify(mSigninMetricsUtilsJniMock)
                .logAccountConsistencyPromoAction(
                        AccountConsistencyPromoAction.GENERIC_ERROR_SHOWN, ACCESS_POINT);
    }

    @Test
    public void testTimeout() {
        mWaiter.waitForCookies(TestAccounts.ACCOUNT1.getId(), mOnCompleteMock);

        triggerWebSigninCallback(WebSigninTrackerResult.TIMEOUT);

        verify(mOnCompleteMock).onResult(PostSigninOperationResult.OTHER_ERROR);
        verify(mSigninMetricsUtilsJniMock)
                .logAccountConsistencyPromoAction(
                        AccountConsistencyPromoAction.GENERIC_ERROR_SHOWN, ACCESS_POINT);
    }

    @Test
    public void testCancelDestroysBridge() {
        mWaiter.waitForCookies(TestAccounts.ACCOUNT1.getId(), mOnCompleteMock);

        mWaiter.cancel();

        // A destroyed bridge doesn't report its result.
        verify(mWebSigninTrackerBridgeMock).destroy();
        verify(mOnCompleteMock, never()).onResult(anyInt());
    }

    @Test
    public void testCancelWithoutPendingWait() {
        mWaiter.cancel();

        verifyNoInteractions(mWebSigninTrackerBridgeFactoryMock, mWebSigninTrackerBridgeMock);
    }

    @Test
    public void testCancelAfterResult() {
        mWaiter.waitForCookies(TestAccounts.ACCOUNT1.getId(), mOnCompleteMock);
        triggerWebSigninCallback(WebSigninTrackerResult.SUCCESS);

        mWaiter.cancel();

        // The bridge destroys itself after reporting its result.
        verify(mWebSigninTrackerBridgeMock, never()).destroy();
    }

    @Test
    public void testNewWaitCancelsPendingWait() {
        WebSigninTrackerBridge secondBridgeMock = mock(WebSigninTrackerBridge.class);
        when(mWebSigninTrackerBridgeFactoryMock.createWithCoreAccountId(
                        eq(mProfileMock), eq(TestAccounts.ACCOUNT2.getId()), any()))
                .thenReturn(secondBridgeMock);
        mWaiter.waitForCookies(TestAccounts.ACCOUNT1.getId(), mOnCompleteMock);

        mWaiter.waitForCookies(TestAccounts.ACCOUNT2.getId(), mOnCompleteMock);

        // The first bridge is destroyed, so it won't report its result.
        verify(mWebSigninTrackerBridgeMock).start();
        verify(mWebSigninTrackerBridgeMock).destroy();
        verify(secondBridgeMock).start();
        verify(secondBridgeMock, never()).destroy();
        verify(mWebSigninTrackerBridgeFactoryMock, times(2))
                .createWithCoreAccountId(
                        eq(mProfileMock), any(), mWebSigninCallbackCaptor.capture());

        mWebSigninCallbackCaptor.getAllValues().get(1).onResult(WebSigninTrackerResult.SUCCESS);
        verify(mOnCompleteMock).onResult(PostSigninOperationResult.SUCCESS);
    }

    @Test
    public void testSynchronousResult() {
        stubSynchronousResult(TestAccounts.ACCOUNT1, WebSigninTrackerResult.SUCCESS);

        mWaiter.waitForCookies(TestAccounts.ACCOUNT1.getId(), mOnCompleteMock);

        verify(mOnCompleteMock).onResult(PostSigninOperationResult.SUCCESS);
        // The bridge destroys itself when reporting its result, even synchronously, so a subsequent
        // cancel() is a no-op.
        mWaiter.cancel();
        verify(mWebSigninTrackerBridgeMock, never()).destroy();
    }

    @Test
    public void testSynchronousResultStartingNewWait() {
        WebSigninTrackerBridge secondBridgeMock = mock(WebSigninTrackerBridge.class);
        when(mWebSigninTrackerBridgeFactoryMock.createWithCoreAccountId(
                        eq(mProfileMock), eq(TestAccounts.ACCOUNT2.getId()), any()))
                .thenReturn(secondBridgeMock);
        stubSynchronousResult(TestAccounts.ACCOUNT1, WebSigninTrackerResult.OTHER_ERROR);
        Callback<@PostSigninOperationResult Integer> retryingCallback =
                result -> mWaiter.waitForCookies(TestAccounts.ACCOUNT2.getId(), mOnCompleteMock);

        mWaiter.waitForCookies(TestAccounts.ACCOUNT1.getId(), retryingCallback);

        // The first bridge destroys itself when reporting its result, and the second wait is still
        // pending.
        verify(mWebSigninTrackerBridgeMock, never()).destroy();
        verify(secondBridgeMock).start();
        verify(secondBridgeMock, never()).destroy();
        mWaiter.cancel();
        verify(secondBridgeMock).destroy();
    }

    private void stubSynchronousResult(
            CoreAccountInfo account, @WebSigninTrackerResult int result) {
        var callback = new AtomicReference<Callback<Integer>>();
        when(mWebSigninTrackerBridgeFactoryMock.createWithCoreAccountId(
                        eq(mProfileMock), eq(account.getId()), any()))
                .thenAnswer(
                        invocation -> {
                            callback.set(invocation.getArgument(2));
                            return mWebSigninTrackerBridgeMock;
                        });
        doAnswer(
                        invocation -> {
                            callback.get().onResult(result);
                            return null;
                        })
                .when(mWebSigninTrackerBridgeMock)
                .start();
    }

    private void triggerWebSigninCallback(@WebSigninTrackerResult int result) {
        verify(mWebSigninTrackerBridgeFactoryMock)
                .createWithCoreAccountId(
                        eq(mProfileMock),
                        eq(TestAccounts.ACCOUNT1.getId()),
                        mWebSigninCallbackCaptor.capture());
        verify(mWebSigninTrackerBridgeMock).start();
        mWebSigninCallbackCaptor.getValue().onResult(result);
    }
}
