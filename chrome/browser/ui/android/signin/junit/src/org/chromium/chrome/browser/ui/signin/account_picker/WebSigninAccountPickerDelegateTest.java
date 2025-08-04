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
import org.robolectric.annotation.LooperMode;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtilsJni;
import org.chromium.chrome.browser.signin.services.WebSigninBridge;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.browser.WebSigninTrackerResult;
import org.chromium.components.signin.metrics.AccountConsistencyPromoAction;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;
import org.chromium.content_public.browser.LoadUrlParams;

/** This class tests the {@link WebSigninAccountPickerDelegate}. */
@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(LooperMode.Mode.LEGACY)
public class WebSigninAccountPickerDelegateTest {
    private static final String CONTINUE_URL = "https://test-continue-url.com";
    private static final String TEST_EMAIL = "test.account@gmail.com";

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

    @Mock private AccountPickerDelegate.SigninStateController mSigninStateControllerMock;

    @Mock private SigninMetricsUtils.Natives mSigninMetricsUtilsJniMock;

    @Captor private ArgumentCaptor<LoadUrlParams> mLoadUrlParamsCaptor;

    @Captor
    private ArgumentCaptor<Callback<@WebSigninTrackerResult Integer>> mWebSigninCallbackCaptor;

    private WebSigninAccountPickerDelegate mDelegate;

    private CoreAccountInfo mCoreAccountInfo;

    @Before
    public void setUp() {
        mCoreAccountInfo = mAccountManagerTestRule.addAccount(TEST_EMAIL);
        when(mTabMock.getProfile()).thenReturn(mProfileMock);
        SigninMetricsUtilsJni.setInstanceForTesting(mSigninMetricsUtilsJniMock);
        mDelegate =
                new WebSigninAccountPickerDelegate(
                        mTabMock, mWebSigninBridgeFactoryMock, CONTINUE_URL);
        when(mWebSigninBridgeFactoryMock.create(eq(mProfileMock), any(), any()))
                .thenReturn(mWebSigninBridgeMock);
    }

    @After
    public void tearDown() {
        mDelegate.onAccountPickerDestroy();
    }

    @Test
    public void testSignInSucceeded() {

        mDelegate.onSignInComplete(mCoreAccountInfo, mSigninStateControllerMock);

        verify(mWebSigninBridgeFactoryMock)
                .create(eq(mProfileMock), eq(mCoreAccountInfo), mWebSigninCallbackCaptor.capture());

        mWebSigninCallbackCaptor.getValue().onResult(WebSigninTrackerResult.SUCCESS);

        verify(mTabMock).loadUrl(mLoadUrlParamsCaptor.capture());
        LoadUrlParams loadUrlParams = mLoadUrlParamsCaptor.getValue();
        Assert.assertEquals("Continue url does not match!", CONTINUE_URL, loadUrlParams.getUrl());
    }

    @Test
    public void testSignInFailedWithConnectionError() {
        mDelegate.onSignInComplete(mCoreAccountInfo, mSigninStateControllerMock);

        verify(mWebSigninBridgeFactoryMock)
                .create(eq(mProfileMock), eq(mCoreAccountInfo), mWebSigninCallbackCaptor.capture());

        mWebSigninCallbackCaptor.getValue().onResult(WebSigninTrackerResult.OTHER_ERROR);

        verify(mSigninStateControllerMock).showGenericError();
        verify(mSigninMetricsUtilsJniMock)
                .logAccountConsistencyPromoAction(
                        AccountConsistencyPromoAction.GENERIC_ERROR_SHOWN,
                        SigninAccessPoint.WEB_SIGNIN);

        // WebSigninBridge should be destroyed after the sign-in result is known.
        verify(mWebSigninBridgeMock).destroy();
    }

    @Test
    public void testSignInFailedWithGaiaError() {
        mDelegate.onSignInComplete(mCoreAccountInfo, mSigninStateControllerMock);

        verify(mWebSigninBridgeFactoryMock)
                .create(eq(mProfileMock), eq(mCoreAccountInfo), mWebSigninCallbackCaptor.capture());

        mWebSigninCallbackCaptor.getValue().onResult(WebSigninTrackerResult.AUTH_ERROR);
        verify(mSigninStateControllerMock).showAuthError();
        verify(mSigninMetricsUtilsJniMock)
                .logAccountConsistencyPromoAction(
                        AccountConsistencyPromoAction.AUTH_ERROR_SHOWN,
                        SigninAccessPoint.WEB_SIGNIN);

        // WebSigninBridge should be destroyed after the sign-in result is known.
        verify(mWebSigninBridgeMock).destroy();
    }
}
