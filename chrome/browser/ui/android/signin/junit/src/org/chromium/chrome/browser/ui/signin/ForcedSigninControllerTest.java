// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.Intent;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.prefs.LocalStatePrefs;
import org.chromium.chrome.browser.prefs.LocalStatePrefsJni;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.identitymanager.PrimaryAccountChangeEvent;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.test.util.TestAccounts;

/** Unit tests for {@link ForcedSigninController}. */
@RunWith(BaseRobolectricTestRunner.class)
@DisableFeatures(SigninFeatures.FORCE_STARTUP_SIGNIN_PROMO)
public class ForcedSigninControllerTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    @Mock private Profile mProfile;
    @Mock private SigninAndHistorySyncActivityLauncher mLauncher;
    @Mock private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock private LocalStatePrefs.Natives mLocalStatePrefsNativeMock;
    @Mock private PrefService mLocalPrefsServiceMock;
    @Mock private Intent mSigninIntent;
    @Mock private Context mContext;

    private ForcedSigninController mController;

    @Before
    public void setUp() {
        LocalStatePrefsJni.setInstanceForTesting(mLocalStatePrefsNativeMock);
        LocalStatePrefs.setNativePrefsLoadedForTesting(true);
        when(mLocalStatePrefsNativeMock.getPrefService()).thenReturn(mLocalPrefsServiceMock);
        when(mLocalPrefsServiceMock.getBoolean(Pref.FORCE_BROWSER_SIGNIN)).thenReturn(false);
        when(mContext.getString(anyInt())).thenReturn("string");

        when(mLauncher.createFullscreenSigninIntent(
                        eq(mContext), eq(mProfile), any(), eq(SigninAccessPoint.FORCED_SIGNIN)))
                .thenReturn(mSigninIntent);

        mController =
                new ForcedSigninController(
                        mContext, mProfile, mLauncher, mActivityLifecycleDispatcher);
    }

    @Test
    @SmallTest
    public void testRegistersActivityLifecycleObserver() {
        verify(mActivityLifecycleDispatcher).register(mController);
    }

    @Test
    @SmallTest
    public void testDestroyUnregistersActivityLifecycleObserver() {
        mController.destroy();

        verify(mActivityLifecycleDispatcher).unregister(mController);
    }

    @Test
    @SmallTest
    @EnableFeatures(SigninFeatures.SUPPORT_FORCED_SIGNIN_POLICY)
    public void testOnResumeWithNative_triggersPromoWhenForcedSigninRequired() {
        when(mLocalPrefsServiceMock.getBoolean(Pref.FORCE_BROWSER_SIGNIN)).thenReturn(true);

        mController.onResumeWithNative();

        verify(mLauncher)
                .createFullscreenSigninIntent(
                        eq(mContext), eq(mProfile), any(), eq(SigninAccessPoint.FORCED_SIGNIN));
        verify(mContext).startActivity(mSigninIntent);
    }

    @Test
    @SmallTest
    @EnableFeatures(SigninFeatures.SUPPORT_FORCED_SIGNIN_POLICY)
    public void testOnResumeWithNative_doesNotTriggerWhenSignedIn() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        mAccountManagerTestRule.getIdentityManager().setPrimaryAccount(TestAccounts.ACCOUNT1);
        when(mLocalPrefsServiceMock.getBoolean(Pref.FORCE_BROWSER_SIGNIN)).thenReturn(true);

        mController.onResumeWithNative();

        verify(mLauncher, never()).createFullscreenSigninIntent(any(), any(), any(), anyInt());
        verify(mContext, never()).startActivity(any());
    }

    @Test
    @SmallTest
    @DisableFeatures(SigninFeatures.SUPPORT_FORCED_SIGNIN_POLICY)
    public void testOnResumeWithNative_doesNotTriggerWhenFeatureDisabled() {
        when(mLocalPrefsServiceMock.getBoolean(Pref.FORCE_BROWSER_SIGNIN)).thenReturn(true);

        mController.onResumeWithNative();

        verify(mLauncher, never()).createFullscreenSigninIntent(any(), any(), any(), anyInt());
        verify(mContext, never()).startActivity(any());
    }

    @Test
    @SmallTest
    @EnableFeatures(SigninFeatures.SUPPORT_FORCED_SIGNIN_POLICY)
    public void testOnResumeWithNative_doesNotTriggerWhenPrefDisabled() {
        when(mLocalPrefsServiceMock.getBoolean(Pref.FORCE_BROWSER_SIGNIN)).thenReturn(false);

        mController.onResumeWithNative();

        verify(mLauncher, never()).createFullscreenSigninIntent(any(), any(), any(), anyInt());
        verify(mContext, never()).startActivity(any());
    }

    @Test
    @SmallTest
    @EnableFeatures(SigninFeatures.SUPPORT_FORCED_SIGNIN_POLICY)
    public void testOnPrimaryAccountCleared_TriggersPromo() {
        when(mLocalPrefsServiceMock.getBoolean(Pref.FORCE_BROWSER_SIGNIN)).thenReturn(true);

        mController.onPrimaryAccountChanged(
                new PrimaryAccountChangeEvent(PrimaryAccountChangeEvent.Type.CLEARED));

        verify(mLauncher)
                .createFullscreenSigninIntent(
                        eq(mContext), eq(mProfile), any(), eq(SigninAccessPoint.FORCED_SIGNIN));
        verify(mContext).startActivity(mSigninIntent);
    }

    @Test
    @SmallTest
    @EnableFeatures(SigninFeatures.SUPPORT_FORCED_SIGNIN_POLICY)
    public void testOnPrimaryAccountSet_doesNotTriggerPromo() {
        when(mLocalPrefsServiceMock.getBoolean(Pref.FORCE_BROWSER_SIGNIN)).thenReturn(true);

        mController.onPrimaryAccountChanged(
                new PrimaryAccountChangeEvent(PrimaryAccountChangeEvent.Type.SET));

        verify(mLauncher, never()).createFullscreenSigninIntent(any(), any(), any(), anyInt());
        verify(mContext, never()).startActivity(any());
    }
}
