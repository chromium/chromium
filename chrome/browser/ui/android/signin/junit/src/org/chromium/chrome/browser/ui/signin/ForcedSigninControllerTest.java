// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.Intent;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.prefs.LocalStatePrefs;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.identitymanager.PrimaryAccountChangeEvent;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.test.util.TestAccounts;

/** Unit tests for {@link ForcedSigninController}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ForcedSigninControllerTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    @Mock private Profile mProfile;
    @Mock private SigninAndHistorySyncActivityLauncher mLauncher;
    @Mock private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock private Intent mSigninIntent;
    @Mock private Context mContext;

    private ForcedSigninController mController;

    @Before
    public void setUp() {
        when(mContext.getString(anyInt())).thenReturn("string");

        when(mLauncher.createFullscreenSigninIntent(
                        eq(mContext), eq(mProfile), any(), eq(SigninAccessPoint.FORCED_SIGNIN)))
                .thenReturn(mSigninIntent);

        mController =
                new ForcedSigninController(
                        mContext, mProfile, mLauncher, mActivityLifecycleDispatcher);
    }

    @Test
    public void testRegistersActivityLifecycleObserver() {
        verify(mActivityLifecycleDispatcher).register(mController);
    }

    @Test
    public void testDestroyUnregistersActivityLifecycleObserver() {
        mController.destroy();

        verify(mActivityLifecycleDispatcher).unregister(mController);
    }

    @Test
    public void testOnResumeWithNative_triggersPromoWhenForcedSigninRequired() {
        LocalStatePrefs.get().setBoolean(Pref.FORCE_BROWSER_SIGNIN, true);

        mController.onResumeWithNative();

        verify(mLauncher)
                .createFullscreenSigninIntent(
                        eq(mContext), eq(mProfile), any(), eq(SigninAccessPoint.FORCED_SIGNIN));
        verify(mContext).startActivity(mSigninIntent);
    }

    @Test
    public void testOnResumeWithNative_doesNotTriggerWhenSignedIn() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        mAccountManagerTestRule.getIdentityManager().setPrimaryAccount(TestAccounts.ACCOUNT1);
        LocalStatePrefs.get().setBoolean(Pref.FORCE_BROWSER_SIGNIN, true);

        mController.onResumeWithNative();

        verify(mLauncher, never()).createFullscreenSigninIntent(any(), any(), any(), anyInt());
        verify(mContext, never()).startActivity(any());
    }

    @Test
    public void testOnResumeWithNative_doesNotTriggerWhenPrefDisabled() {
        LocalStatePrefs.get().setBoolean(Pref.FORCE_BROWSER_SIGNIN, false);

        mController.onResumeWithNative();

        verify(mLauncher, never()).createFullscreenSigninIntent(any(), any(), any(), anyInt());
        verify(mContext, never()).startActivity(any());
    }

    @Test
    public void testOnPrimaryAccountCleared_TriggersPromo() {
        LocalStatePrefs.get().setBoolean(Pref.FORCE_BROWSER_SIGNIN, true);

        mController.onPrimaryAccountChanged(
                new PrimaryAccountChangeEvent(PrimaryAccountChangeEvent.Type.CLEARED));

        verify(mLauncher)
                .createFullscreenSigninIntent(
                        eq(mContext), eq(mProfile), any(), eq(SigninAccessPoint.FORCED_SIGNIN));
        verify(mContext).startActivity(mSigninIntent);
    }

    @Test
    public void testOnPrimaryAccountSet_doesNotTriggerPromo() {
        LocalStatePrefs.get().setBoolean(Pref.FORCE_BROWSER_SIGNIN, true);

        mController.onPrimaryAccountChanged(
                new PrimaryAccountChangeEvent(PrimaryAccountChangeEvent.Type.SET));

        verify(mLauncher, never()).createFullscreenSigninIntent(any(), any(), any(), anyInt());
        verify(mContext, never()).startActivity(any());
    }

    @Test
    public void testShowFullscreenSigninPromptIfRequired() {
        LocalStatePrefs.get().setBoolean(Pref.FORCE_BROWSER_SIGNIN, true);

        assertTrue(mController.showFullscreenSigninPromptIfRequired());

        verify(mLauncher)
                .createFullscreenSigninIntent(
                        eq(mContext), eq(mProfile), any(), eq(SigninAccessPoint.FORCED_SIGNIN));
        verify(mContext).startActivity(mSigninIntent);
    }

    @Test
    public void testShowFullscreenSigninPromptIfRequired_doesNotTriggerWhenPrefDisabled() {
        LocalStatePrefs.get().setBoolean(Pref.FORCE_BROWSER_SIGNIN, false);

        assertFalse(mController.showFullscreenSigninPromptIfRequired());

        verify(mLauncher, never()).createFullscreenSigninIntent(any(), any(), any(), anyInt());
        verify(mContext, never()).startActivity(any());
    }

    /**
     * The controller prompt is driven by the forced sign-in policy only: the startup promo flag is
     * handled separately by {@link FullscreenSigninPromoLauncher#launchPromoIfForced}.
     */
    @Test
    @EnableFeatures(SigninFeatures.FORCE_STARTUP_SIGNIN_PROMO)
    public void testShowFullscreenSigninPromptIfRequired_ignoresStartupPromoFeature() {
        assertFalse(mController.showFullscreenSigninPromptIfRequired());

        verify(mLauncher, never()).createFullscreenSigninIntent(any(), any(), any(), anyInt());
        verify(mContext, never()).startActivity(any());
    }
}
