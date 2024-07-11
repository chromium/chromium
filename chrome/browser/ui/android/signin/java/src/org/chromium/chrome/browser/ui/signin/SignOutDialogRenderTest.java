// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.when;

import android.view.View;

import androidx.test.filters.LargeTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridge;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridgeJni;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SignoutReason;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.ui.test.util.BlankUiTestActivity;

/** Render tests for {@link SignOutDialogCoordinator} */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class SignOutDialogRenderTest {
    private static final String TEST_DOMAIN = "test.domain.example.com";

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.SERVICES_SIGN_IN)
                    .build();

    @Rule
    public final BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule public final JniMocker mocker = new JniMocker();

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private PasswordManagerUtilBridge.Natives mPasswordManagerUtilBridgeNativeMock;

    @Mock private IdentityServicesProvider mIdentityServicesProvider;

    @Mock private SigninManager mSigninManagerMock;

    @Mock private IdentityManager mIdentityManagerMock;

    @Mock private Profile mProfile;

    @Mock private UserPrefs.Natives mUserPrefsMock;

    @Mock private PrefService mPrefService;

    private SignOutDialogCoordinator mSignOutDialogCoordinator;

    @Before
    public void setUp() {
        mocker.mock(PasswordManagerUtilBridgeJni.TEST_HOOKS, mPasswordManagerUtilBridgeNativeMock);
        mocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsMock);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        when(mIdentityServicesProvider.getSigninManager(any())).thenReturn(mSigninManagerMock);
        when(mIdentityServicesProvider.getIdentityManager(any())).thenReturn(mIdentityManagerMock);
        when(mIdentityManagerMock.hasPrimaryAccount(ConsentLevel.SYNC)).thenReturn(true);
        mActivityTestRule.launchActivity(null);
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testSignOutDialogForNonSyncingAccount() throws Exception {
        mockAllowDeletingBrowserHistoryPref(true);
        when(mIdentityManagerMock.hasPrimaryAccount(ConsentLevel.SYNC)).thenReturn(false);

        mRenderTestRule.render(showSignOutDialog(), "signout_dialog_for_non_syncing_account");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testTurnOffSyncDialogForNonSyncingAccount() throws Exception {
        mockAllowDeletingBrowserHistoryPref(true);
        when(mIdentityManagerMock.hasPrimaryAccount(ConsentLevel.SYNC)).thenReturn(false);

        mRenderTestRule.render(
                showTurnOffSyncDialog(), "turn_off_sync_dialog_for_non_syncing_account");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testSignOutDialogForNonSyncingManagedAccount() throws Exception {
        mockAllowDeletingBrowserHistoryPref(true);
        when(mIdentityManagerMock.hasPrimaryAccount(ConsentLevel.SYNC)).thenReturn(false);
        when(mSigninManagerMock.getManagementDomain()).thenReturn(TEST_DOMAIN);

        mRenderTestRule.render(
                showSignOutDialog(), "signout_dialog_for_non_syncing_managed_account");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testSignOutDialogForManagedAccount() throws Exception {
        mockAllowDeletingBrowserHistoryPref(true);
        when(mSigninManagerMock.getManagementDomain()).thenReturn(TEST_DOMAIN);

        mRenderTestRule.render(showSignOutDialog(), "signout_dialog_for_managed_account");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testSignOutDialogForManagedAccountCannotDeleteHistory() throws Exception {
        mockAllowDeletingBrowserHistoryPref(false);
        when(mSigninManagerMock.getManagementDomain()).thenReturn(TEST_DOMAIN);

        mRenderTestRule.render(
                showSignOutDialog(), "signout_dialog_for_managed_account_cannot_delete_history");
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

        mRenderTestRule.render(
                showTurnOffSyncDialog(),
                "turn_off_sync_dialog_for_non_managed_account_cannot_delete_history");
    }

    private View showTurnOffSyncDialog() throws Exception {
        when(mProfile.isChild()).thenReturn(true);
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSignOutDialogCoordinator =
                            new SignOutDialogCoordinator(
                                    mActivityTestRule.getActivity(),
                                    mProfile,
                                    mActivityTestRule.getActivity().getSupportFragmentManager(),
                                    mActivityTestRule.getActivity().getModalDialogManager(),
                                    SignoutReason.USER_CLICKED_REVOKE_SYNC_CONSENT_SETTINGS,
                                    null);
                    return mSignOutDialogCoordinator.getDialogViewForTesting();
                });
    }

    private View showSignOutDialog() throws Exception {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSignOutDialogCoordinator =
                            new SignOutDialogCoordinator(
                                    mActivityTestRule.getActivity(),
                                    mProfile,
                                    mActivityTestRule.getActivity().getSupportFragmentManager(),
                                    mActivityTestRule.getActivity().getModalDialogManager(),
                                    SignoutReason.USER_CLICKED_SIGNOUT_SETTINGS,
                                    null);
                    return mSignOutDialogCoordinator.getDialogViewForTesting();
                });
    }

    private void mockAllowDeletingBrowserHistoryPref(boolean value) {
        when(mUserPrefsMock.get(mProfile)).thenReturn(mPrefService);
        when(mPrefService.getBoolean(Pref.ALLOW_DELETING_BROWSER_HISTORY)).thenReturn(value);
    }
}
