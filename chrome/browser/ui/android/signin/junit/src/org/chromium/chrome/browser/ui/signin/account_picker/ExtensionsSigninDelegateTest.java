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
    @Mock private Runnable mOnFlowFinishedMock;

    @Captor
    private ArgumentCaptor<Callback<@WebSigninTrackerResult Integer>> mWebSigninCallbackCaptor;

    private static final SigninAndHistorySyncCoordinator.Result SIGNED_IN =
            new SigninAndHistorySyncCoordinator.Result(
                    /* hasSignedIn= */ true, /* hasOptedInHistorySync= */ false);

    private ExtensionsSigninDelegate mDelegate;

    @Before
    public void setUp() {
        SigninMetricsUtilsJni.setInstanceForTesting(mSigninMetricsUtilsJniMock);
        mDelegate = new ExtensionsSigninDelegate(mProfileMock, mWebSigninTrackerBridgeFactoryMock);
    }

    @Test
    public void testRunPostSigninAction_success() {
        stubWebSigninTrackerBridgeFactory();
        mDelegate.runPostSigninAction(
                TestAccounts.ACCOUNT1, /* delegateContext= */ null, mPostSigninCallbackMock);

        triggerWebSigninCallback(WebSigninTrackerResult.SUCCESS);

        verify(mPostSigninCallbackMock).onResult(PostSigninOperationResult.SUCCESS);
        verifyNoInteractions(mSigninMetricsUtilsJniMock);
    }

    @Test
    public void testRunPostSigninAction_authError() {
        stubWebSigninTrackerBridgeFactory();
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
        stubWebSigninTrackerBridgeFactory();
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
        stubWebSigninTrackerBridgeFactory();
        mDelegate.runPostSigninAction(
                TestAccounts.ACCOUNT1, /* delegateContext= */ null, mPostSigninCallbackMock);

        // E.g. the user dismisses the bottom sheet during the wait.
        mDelegate.onFlowComplete(SigninAndHistorySyncCoordinator.Result.aborted());

        // A destroyed bridge doesn't report its result.
        verify(mWebSigninTrackerBridgeMock).destroy();
        verifyNoInteractions(mPostSigninCallbackMock, mSigninMetricsUtilsJniMock);
    }

    @Test
    public void testOnFlowFinished_signedIn() {
        mDelegate.setOnFlowFinishedCallback(mOnFlowFinishedMock);
        stubWebSigninTrackerBridgeFactory();
        mDelegate.runPostSigninAction(
                TestAccounts.ACCOUNT1, /* delegateContext= */ null, mPostSigninCallbackMock);
        triggerWebSigninCallback(WebSigninTrackerResult.SUCCESS);
        verifyNoInteractions(mOnFlowFinishedMock);

        mDelegate.onFlowComplete(SIGNED_IN);

        verify(mOnFlowFinishedMock).run();
    }

    @Test
    public void testOnFlowFinished_aborted() {
        mDelegate.setOnFlowFinishedCallback(mOnFlowFinishedMock);
        stubWebSigninTrackerBridgeFactory();
        mDelegate.runPostSigninAction(
                TestAccounts.ACCOUNT1, /* delegateContext= */ null, mPostSigninCallbackMock);

        // E.g. the user dismisses the bottom sheet during the wait.
        mDelegate.onFlowComplete(SigninAndHistorySyncCoordinator.Result.aborted());

        verify(mOnFlowFinishedMock).run();
    }

    @Test
    public void testOnFlowFinished_runsCallbackOnlyOnce() {
        mDelegate.setOnFlowFinishedCallback(mOnFlowFinishedMock);
        mDelegate.onFlowComplete(SigninAndHistorySyncCoordinator.Result.aborted());

        // E.g. a flow started from the "add account" activity result, without callback.
        mDelegate.onFlowComplete(SigninAndHistorySyncCoordinator.Result.aborted());

        verify(mOnFlowFinishedMock).run();
    }

    @Test
    public void testOnSigninCoordinatorDestroyed_reportsPendingCallbackAndCancelsWait() {
        mDelegate.setOnFlowFinishedCallback(mOnFlowFinishedMock);
        stubWebSigninTrackerBridgeFactory();
        mDelegate.runPostSigninAction(
                TestAccounts.ACCOUNT1, /* delegateContext= */ null, mPostSigninCallbackMock);

        mDelegate.onSigninCoordinatorDestroyed();

        verify(mOnFlowFinishedMock).run();
        verify(mWebSigninTrackerBridgeMock).destroy();
    }

    private void stubWebSigninTrackerBridgeFactory() {
        when(mWebSigninTrackerBridgeFactoryMock.createWithCoreAccountId(
                        eq(mProfileMock), any(), any()))
                .thenReturn(mWebSigninTrackerBridgeMock);
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
