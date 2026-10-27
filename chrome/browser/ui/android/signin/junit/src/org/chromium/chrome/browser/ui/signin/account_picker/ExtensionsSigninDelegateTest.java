// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.account_picker;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
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
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncCoordinator;
import org.chromium.components.signin.browser.WebSigninTrackerResult;
import org.chromium.components.signin.metrics.AccountConsistencyPromoAction;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.test.util.TestAccounts;

/** Tests for {@link ExtensionsSigninDelegate}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ExtensionsSigninDelegateTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private WebSigninTrackerBridge.Factory mWebSigninTrackerBridgeFactoryMock;
    @Mock private WebSigninTrackerBridge mWebSigninTrackerBridgeMock;
    @Mock private Profile mProfileMock;
    @Mock private SigninMetricsUtils.Natives mSigninMetricsUtilsJniMock;
    @Mock private Callback<@PostSigninOperationResult Integer> mPostSigninCallbackMock;

    @Captor
    private ArgumentCaptor<Callback<@WebSigninTrackerResult Integer>> mWebSigninCallbackCaptor;

    private ExtensionsSigninDelegate mDelegate;

    @Before
    public void setUp() {
        SigninMetricsUtilsJni.setInstanceForTesting(mSigninMetricsUtilsJniMock);
        when(mWebSigninTrackerBridgeFactoryMock.createWithCoreAccountId(
                        eq(mProfileMock), any(), any()))
                .thenReturn(mWebSigninTrackerBridgeMock);
        mDelegate = new ExtensionsSigninDelegate(mProfileMock, mWebSigninTrackerBridgeFactoryMock);
    }

    @Test
    public void testRunPostSigninAction_success() {
        mDelegate.runPostSigninAction(
                TestAccounts.ACCOUNT1, /* delegateContext= */ null, mPostSigninCallbackMock);

        triggerWebSigninCallback(WebSigninTrackerResult.SUCCESS);

        verify(mPostSigninCallbackMock).onResult(PostSigninOperationResult.SUCCESS);
        verifyNoInteractions(mSigninMetricsUtilsJniMock);
    }

    @Test
    public void testRunPostSigninAction_authError() {
        mDelegate.runPostSigninAction(
                TestAccounts.ACCOUNT1, /* delegateContext= */ null, mPostSigninCallbackMock);

        triggerWebSigninCallback(WebSigninTrackerResult.AUTH_ERROR);

        verify(mPostSigninCallbackMock).onResult(PostSigninOperationResult.AUTH_ERROR);
        verify(mSigninMetricsUtilsJniMock)
                .logAccountConsistencyPromoAction(
                        AccountConsistencyPromoAction.AUTH_ERROR_SHOWN,
                        SigninAccessPoint.EXTENSIONS);
    }

    @Test
    public void testRunPostSigninAction_otherError() {
        mDelegate.runPostSigninAction(
                TestAccounts.ACCOUNT1, /* delegateContext= */ null, mPostSigninCallbackMock);

        triggerWebSigninCallback(WebSigninTrackerResult.OTHER_ERROR);

        verify(mPostSigninCallbackMock).onResult(PostSigninOperationResult.OTHER_ERROR);
        verify(mSigninMetricsUtilsJniMock)
                .logAccountConsistencyPromoAction(
                        AccountConsistencyPromoAction.GENERIC_ERROR_SHOWN,
                        SigninAccessPoint.EXTENSIONS);
    }

    @Test
    public void testOnFlowComplete_cancelsPendingWait() {
        mDelegate.runPostSigninAction(
                TestAccounts.ACCOUNT1, /* delegateContext= */ null, mPostSigninCallbackMock);

        // E.g. the user dismisses the bottom sheet during the wait.
        mDelegate.onFlowComplete(SigninAndHistorySyncCoordinator.Result.aborted());

        // A destroyed bridge doesn't report its result.
        verify(mWebSigninTrackerBridgeMock).destroy();
        verifyNoInteractions(mPostSigninCallbackMock, mSigninMetricsUtilsJniMock);
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
