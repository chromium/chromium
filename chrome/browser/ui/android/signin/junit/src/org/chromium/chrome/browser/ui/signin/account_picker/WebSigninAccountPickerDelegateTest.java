// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.account_picker;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.After;
import org.junit.Assert;
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
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtilsJni;
import org.chromium.chrome.browser.signin.services.WebSigninBridge;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.signin.DelegateContext;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.signin.browser.WebSigninTrackerResult;
import org.chromium.components.signin.metrics.AccountConsistencyPromoAction;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.url.GURL;

/** This class tests the {@link WebSigninAccountPickerDelegate}. */
@RunWith(BaseRobolectricTestRunner.class)
public class WebSigninAccountPickerDelegateTest {
    private static final GURL CONTINUE_URL = new GURL("https://test-continue-url.com");
    private static final int TAB_ID = 123;
    private static final DelegateContext DELEGATE_CONTEXT =
            new WebSigninDelegateContext(TAB_ID, CONTINUE_URL);

    private final FakeAccountManagerFacade mFakeAccountManagerFacade =
            spy(new FakeAccountManagerFacade());

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule =
            new AccountManagerTestRule(mFakeAccountManagerFacade);

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private WebSigninBridge.Factory mWebSigninBridgeFactoryMock;

    @Mock private WebSigninBridge mWebSigninBridgeMock;

    @Mock private Profile mProfileMock;

    @Mock private Tab mTabMock;

    @Mock private TabModelSelector mTabModelSelectorMock;

    @Mock private AccountPickerDelegate.SigninStateController mSigninStateControllerMock;

    @Mock private SigninMetricsUtils.Natives mSigninMetricsUtilsJniMock;

    @Mock private Callback<@PostSigninOperationResult Integer> mPostSigninCallbackMock;

    @Captor private ArgumentCaptor<LoadUrlParams> mLoadUrlParamsCaptor;

    @Captor
    private ArgumentCaptor<Callback<@WebSigninTrackerResult Integer>> mWebSigninCallbackCaptor;

    private @Nullable WebSigninAccountPickerDelegate mDelegate;

    @Before
    public void setUp() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        SigninMetricsUtilsJni.setInstanceForTesting(mSigninMetricsUtilsJniMock);
        when(mWebSigninBridgeFactoryMock.createWithCoreAccountId(eq(mProfileMock), any(), any()))
                .thenReturn(mWebSigninBridgeMock);
    }

    @After
    public void tearDown() {
        if (mDelegate != null) {
            mDelegate.onAccountPickerDestroy();
        }
    }

    @Test
    public void testSignInSucceeded() {
        when(mTabModelSelectorMock.getTabById(TAB_ID)).thenReturn(mTabMock);
        mDelegate =
                new WebSigninAccountPickerDelegate(
                        mProfileMock, mTabModelSelectorMock, mWebSigninBridgeFactoryMock);

        mDelegate.runPostSigninAction(
                TestAccounts.ACCOUNT1, DELEGATE_CONTEXT, mPostSigninCallbackMock);

        verifyWebSigninBridgeAndTriggerCallback(WebSigninTrackerResult.SUCCESS);

        verify(mPostSigninCallbackMock).onResult(PostSigninOperationResult.SUCCESS);
    }

    // TODO(crbug.com/469772349): Remove all legacy tests (using deprecated constructor) after
    // activityless-signin migration.

    @Test
    public void testSignInSucceeded_legacy() {
        when(mTabMock.getProfile()).thenReturn(mProfileMock);
        mDelegate =
                new WebSigninAccountPickerDelegate(
                        mTabMock, mWebSigninBridgeFactoryMock, CONTINUE_URL);

        mDelegate.onSignInComplete(TestAccounts.ACCOUNT1, mSigninStateControllerMock);

        verifyWebSigninBridgeAndTriggerCallback(WebSigninTrackerResult.SUCCESS);

        verify(mSigninStateControllerMock).onSigninComplete();
    }

    @Test
    public void testSignInFailedWithConnectionError() {
        mDelegate =
                new WebSigninAccountPickerDelegate(
                        mProfileMock, mTabModelSelectorMock, mWebSigninBridgeFactoryMock);

        mDelegate.runPostSigninAction(
                TestAccounts.ACCOUNT1, DELEGATE_CONTEXT, mPostSigninCallbackMock);

        verifyWebSigninBridgeAndTriggerCallback(WebSigninTrackerResult.OTHER_ERROR);

        verify(mPostSigninCallbackMock).onResult(PostSigninOperationResult.OTHER_ERROR);
    }

    @Test
    public void testSignInFailedWithConnectionError_legacy() {
        when(mTabMock.getProfile()).thenReturn(mProfileMock);
        mDelegate =
                new WebSigninAccountPickerDelegate(
                        mTabMock, mWebSigninBridgeFactoryMock, CONTINUE_URL);

        mDelegate.onSignInComplete(TestAccounts.ACCOUNT1, mSigninStateControllerMock);

        verifyWebSigninBridgeAndTriggerCallback(WebSigninTrackerResult.OTHER_ERROR);

        verify(mSigninStateControllerMock).showGenericError();
    }

    @Test
    public void testSignInFailedWithGaiaError() {
        mDelegate =
                new WebSigninAccountPickerDelegate(
                        mProfileMock, mTabModelSelectorMock, mWebSigninBridgeFactoryMock);

        mDelegate.runPostSigninAction(
                TestAccounts.ACCOUNT1, DELEGATE_CONTEXT, mPostSigninCallbackMock);

        verifyWebSigninBridgeAndTriggerCallback(WebSigninTrackerResult.AUTH_ERROR);

        verify(mPostSigninCallbackMock).onResult(PostSigninOperationResult.AUTH_ERROR);
    }

    @Test
    public void testSignInFailedWithGaiaError_legacy() {
        when(mTabMock.getProfile()).thenReturn(mProfileMock);
        mDelegate =
                new WebSigninAccountPickerDelegate(
                        mTabMock, mWebSigninBridgeFactoryMock, CONTINUE_URL);

        mDelegate.onSignInComplete(TestAccounts.ACCOUNT1, mSigninStateControllerMock);

        verifyWebSigninBridgeAndTriggerCallback(WebSigninTrackerResult.AUTH_ERROR);
        verify(mSigninStateControllerMock).showAuthError();
    }

    private void verifyWebSigninBridgeAndTriggerCallback(@WebSigninTrackerResult int result) {
        verify(mWebSigninBridgeFactoryMock)
                .createWithCoreAccountId(
                        eq(mProfileMock),
                        eq(TestAccounts.ACCOUNT1.getId()),
                        mWebSigninCallbackCaptor.capture());

        mWebSigninCallbackCaptor.getValue().onResult(result);

        if (result == WebSigninTrackerResult.SUCCESS) {
            verify(mTabMock).loadUrl(mLoadUrlParamsCaptor.capture());
            LoadUrlParams loadUrlParams = mLoadUrlParamsCaptor.getValue();
            Assert.assertEquals(
                    "Continue url does not match!", CONTINUE_URL.getSpec(), loadUrlParams.getUrl());
        } else if (result == WebSigninTrackerResult.OTHER_ERROR) {
            verify(mSigninMetricsUtilsJniMock)
                    .logAccountConsistencyPromoAction(
                            AccountConsistencyPromoAction.GENERIC_ERROR_SHOWN,
                            SigninAccessPoint.WEB_SIGNIN);
        } else if (result == WebSigninTrackerResult.AUTH_ERROR) {
            verify(mSigninMetricsUtilsJniMock)
                    .logAccountConsistencyPromoAction(
                            AccountConsistencyPromoAction.AUTH_ERROR_SHOWN,
                            SigninAccessPoint.WEB_SIGNIN);
        } else {
            throw new IllegalStateException("Unexpected result: " + result);
        }
    }
}
