// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

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

import org.chromium.base.test.BaseActivityTestRule;
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
import org.chromium.chrome.browser.ui.signin.SignOutDialogCoordinator.ActionType;
import org.chromium.chrome.browser.ui.signin.SignOutDialogCoordinator.Listener;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.GAIAServiceType;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.BlankUiTestActivity;

/**
 * Render tests for {@link SignOutDialogCoordinator}
 */
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

    @Rule
    public final JniMocker mocker = new JniMocker();

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock
    private SigninMetricsUtils.Natives mSigninMetricsUtilsNativeMock;

    @Mock
    private SigninManager mSigninManagerMock;

    @Mock
    private IdentityManager mIdentityManagerMock;

    @Mock
    private Profile mProfile;

    @Mock
    private UserPrefs.Natives mUserPrefsMock;

    @Mock
    private PrefService mPrefService;

    @Mock
    private Listener mListenerMock;

    private SignOutDialogCoordinator mSignOutDialogCoordinator;

    @Before
    public void setUp() {
        mocker.mock(SigninMetricsUtilsJni.TEST_HOOKS, mSigninMetricsUtilsNativeMock);
        Profile.setLastUsedProfileForTesting(mProfile);
        IdentityServicesProvider.setInstanceForTests(mock(IdentityServicesProvider.class));
        when(IdentityServicesProvider.get().getSigninManager(any())).thenReturn(mSigninManagerMock);
        when(IdentityServicesProvider.get().getIdentityManager(any()))
                .thenReturn(mIdentityManagerMock);
        when(mIdentityManagerMock.hasPrimaryAccount(ConsentLevel.SYNC)).thenReturn(true);
        mocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsMock);
        mActivityTestRule.launchActivity(null);
    }

    @After
    public void tearDown() {
        // Since the Dialog dismiss calls native method, we need to close the dialog before the
        // Native mock SigninMetricsUtils.Natives gets removed.
        if (mSignOutDialogCoordinator != null) {
            TestThreadUtils.runOnUiThreadBlocking(
                    mSignOutDialogCoordinator::dismissDialogForTesting);
        }
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
    public void testSignOutDialogForNonSyncingManagedAccount() throws Exception {
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

        mRenderTestRule.render(showSignOutDialog(), "signout_dialog_for_non_syncing_account");
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

        mRenderTestRule.render(showTurnOffSyncDialog(),
                "signout_dialog_for_managed_account_cannot_delete_history");
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

    private View showTurnOffSyncDialog() throws Exception {
        return TestThreadUtils.runOnUiThreadBlocking(() -> {
            mSignOutDialogCoordinator =
                    new SignOutDialogCoordinator(mActivityTestRule.getActivity(),
                            mActivityTestRule.getActivity().getModalDialogManager(), mListenerMock,
                            ActionType.REVOKE_SYNC_CONSENT, GAIAServiceType.GAIA_SERVICE_TYPE_NONE);
            return mSignOutDialogCoordinator.getDialogViewForTesting();
        });
    }

    private View showSignOutDialog() throws Exception {
        return TestThreadUtils.runOnUiThreadBlocking(() -> {
            mSignOutDialogCoordinator = new SignOutDialogCoordinator(
                    mActivityTestRule.getActivity(),
                    mActivityTestRule.getActivity().getModalDialogManager(), mListenerMock,
                    ActionType.CLEAR_PRIMARY_ACCOUNT, GAIAServiceType.GAIA_SERVICE_TYPE_NONE);
            return mSignOutDialogCoordinator.getDialogViewForTesting();
        });
    }

    private void mockAllowDeletingBrowserHistoryPref(boolean value) {
        when(mUserPrefsMock.get(mProfile)).thenReturn(mPrefService);
        when(mPrefService.getBoolean(Pref.ALLOW_DELETING_BROWSER_HISTORY)).thenReturn(value);
    }
}
