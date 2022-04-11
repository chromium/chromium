// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.support.test.InstrumentationRegistry;
import android.view.View;

import androidx.test.filters.LargeTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtilsJni;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.GAIAServiceType;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;

/**
 * Render tests for {@link SignOutDialogFragment}
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class SignOutDialogRenderTest extends BlankUiTestActivityTestCase {
    private static final String TEST_DOMAIN = "test.domain.example.com";

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.SERVICES_SIGN_IN)
                    .build();

    @Rule
    public final JniMocker mocker = new JniMocker();

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock
    private SigninMetricsUtils.Natives mSigninMetricsUtilsNativeMock;

    @Mock
    private SigninManager mSigninManagerMock;

    @Mock
    private Profile mProfile;

    @Mock
    private UserPrefs.Natives mUserPrefsMock;

    @Mock
    private PrefService mPrefService;

    private SignOutDialogFragment mSignOutDialog;

    @Before
    public void setUp() {
        mocker.mock(SigninMetricsUtilsJni.TEST_HOOKS, mSigninMetricsUtilsNativeMock);
        Profile.setLastUsedProfileForTesting(mProfile);
        IdentityServicesProvider.setInstanceForTests(mock(IdentityServicesProvider.class));
        when(IdentityServicesProvider.get().getSigninManager(any())).thenReturn(mSigninManagerMock);
        mocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsMock);
    }

    @After
    public void tearDown() {
        // Since the Dialog dismiss calls native method, we need to close the dialog before the
        // Native mock SigninMetricsUtils.Natives gets removed.
        if (mSignOutDialog != null) {
            TestThreadUtils.runOnUiThreadBlocking(() -> mSignOutDialog.dismiss());
        }
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testSignOutDialogForManagedAccount() throws Exception {
        when(mSigninManagerMock.getManagementDomain()).thenReturn(TEST_DOMAIN);
        mRenderTestRule.render(showSignOutDialog(), "signout_dialog_for_managed_account");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testSignOutDialogForNonManagedAccount() throws Exception {
        mockAllowDeletingBrowserHistoryPref(true);
        mRenderTestRule.render(showSignOutDialog(), "signout_dialog_for_non_managed_account");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testTurnOffSyncDialogForNonManagedAccount() throws Exception {
        mockAllowDeletingBrowserHistoryPref(true);
        mRenderTestRule.render(
                showTurnOffSyncDialog(), "turn_off_sync_dialog_for_non_managed_account");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testTurnOffSyncDialogForNonManagedAccountCannotDeleteHistory() throws Exception {
        mockAllowDeletingBrowserHistoryPref(false);
        mRenderTestRule.render(showTurnOffSyncDialog(),
                "turn_off_sync_dialog_for_non_managed_account_cannot_delete_history");
    }

    private View showTurnOffSyncDialog() {
        mSignOutDialog =
                SignOutDialogFragment.create(SignOutDialogFragment.ActionType.REVOKE_SYNC_CONSENT,
                        GAIAServiceType.GAIA_SERVICE_TYPE_NONE);
        mSignOutDialog.show(getActivity().getSupportFragmentManager(), null);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        return mSignOutDialog.getDialog().getWindow().getDecorView();
    }

    private View showSignOutDialog() {
        mSignOutDialog =
                SignOutDialogFragment.create(SignOutDialogFragment.ActionType.CLEAR_PRIMARY_ACCOUNT,
                        GAIAServiceType.GAIA_SERVICE_TYPE_NONE);
        mSignOutDialog.show(getActivity().getSupportFragmentManager(), null);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        return mSignOutDialog.getDialog().getWindow().getDecorView();
    }

    private void mockAllowDeletingBrowserHistoryPref(boolean value) {
        when(mUserPrefsMock.get(mProfile)).thenReturn(mPrefService);
        when(mPrefService.getBoolean(Pref.ALLOW_DELETING_BROWSER_HISTORY)).thenReturn(value);
    }
}
