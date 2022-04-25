// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.widget.TextView;

import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.Robolectric;
import org.robolectric.annotation.LooperMode;
import org.robolectric.shadows.ShadowAlertDialog;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileAccountManagementMetrics;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtilsJni;
import org.chromium.chrome.browser.ui.signin.SignOutDialogFragment.SignOutDialogListener;
import org.chromium.components.browser_ui.test.BrowserUiDummyFragmentActivity;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.GAIAServiceType;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;

/** Tests for {@link SignOutDialogFragment}. */
@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(LooperMode.Mode.LEGACY)
public class SignOutDialogFragmentTest {
    private static final String TEST_DOMAIN = "test.domain.example.com";

    /** Dummy {@link Fragment} used only for this test class. */
    public static class DummySignOutTargetFragment
            extends Fragment implements SignOutDialogListener {
        private SignOutDialogListener mListener;

        @Override
        public void onSignOutClicked(boolean forceWipeUserData) {
            mListener.onSignOutClicked(forceWipeUserData);
        }

        void setListener(SignOutDialogListener listener) {
            mListener = listener;
        }
    }

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
    private UserPrefs.Natives mUserPrefsNatives;

    @Mock
    private PrefService mPrefService;

    @Mock
    private SignOutDialogListener mSignOutDialogListenerMock;

    private final DummySignOutTargetFragment mTargetFragment = new DummySignOutTargetFragment();

    private SignOutDialogFragment mSignOutDialog;

    private FragmentManager mFragmentManager;

    @Before
    public void setUp() {
        mocker.mock(SigninMetricsUtilsJni.TEST_HOOKS, mSigninMetricsUtilsNativeMock);
        mocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsNatives);
        IdentityServicesProvider.setInstanceForTests(mock(IdentityServicesProvider.class));
        Profile.setLastUsedProfileForTesting(mProfile);
        when(IdentityServicesProvider.get().getSigninManager(any())).thenReturn(mSigninManagerMock);
        setUpSignOutDialog();
    }

    private void setUpSignOutDialog() {
        mFragmentManager = Robolectric.buildActivity(BrowserUiDummyFragmentActivity.class)
                                   .setup()
                                   .get()
                                   .getSupportFragmentManager();
        mTargetFragment.setListener(mSignOutDialogListenerMock);
        mFragmentManager.beginTransaction().add(mTargetFragment, null).commit();
        mSignOutDialog =
                SignOutDialogFragment.create(SignOutDialogFragment.ActionType.CLEAR_PRIMARY_ACCOUNT,
                        GAIAServiceType.GAIA_SERVICE_TYPE_NONE);
        mSignOutDialog.setTargetFragment(mTargetFragment, 0);
    }

    @Test
    public void testMessageWhenAccountIsManaged() {
        when(mSigninManagerMock.getManagementDomain()).thenReturn(TEST_DOMAIN);
        AlertDialog alertDialog = showSignOutAlertDialog();
        TextView messageTextView = alertDialog.findViewById(android.R.id.message);
        assertEquals(
                mSignOutDialog.getString(R.string.signout_managed_account_message, TEST_DOMAIN),
                messageTextView.getText());
    }

    @Test
    public void testNoDataWipeCheckboxWhenAccountIsManaged() {
        when(mSigninManagerMock.getManagementDomain()).thenReturn(TEST_DOMAIN);
        AlertDialog alertDialog = showSignOutAlertDialog();
        assertNull(alertDialog.findViewById(R.id.remove_local_data));
    }

    @Test
    public void testPositiveButtonWhenAccountIsManaged() {
        when(mSigninManagerMock.getManagementDomain()).thenReturn(TEST_DOMAIN);
        AlertDialog alertDialog = showSignOutAlertDialog();
        alertDialog.getButton(AlertDialog.BUTTON_POSITIVE).performClick();
        verify(mSigninMetricsUtilsNativeMock)
                .logProfileAccountManagementMenu(ProfileAccountManagementMetrics.SIGNOUT_SIGNOUT,
                        GAIAServiceType.GAIA_SERVICE_TYPE_NONE);
        verify(mSignOutDialogListenerMock).onSignOutClicked(false);
    }

    @Test
    public void testNoDataWipeCheckboxWhenAccountIsNotManagedAndHistoryDeletionNotAllowed() {
        mockAllowDeletingBrowserHistoryPref(false);

        AlertDialog alertDialog = showSignOutAlertDialog();
        assertNull(alertDialog.findViewById(R.id.remove_local_data));
    }

    @Test
    public void testPositiveButtonWhenAccountIsNotManagedAndHistoryDeletionNotAllowed() {
        mockAllowDeletingBrowserHistoryPref(false);

        AlertDialog alertDialog = showSignOutAlertDialog();
        alertDialog.getButton(AlertDialog.BUTTON_POSITIVE).performClick();
        verify(mSigninMetricsUtilsNativeMock)
                .logProfileAccountManagementMenu(ProfileAccountManagementMetrics.SIGNOUT_SIGNOUT,
                        GAIAServiceType.GAIA_SERVICE_TYPE_NONE);
        verify(mSignOutDialogListenerMock).onSignOutClicked(false);
    }

    @Test
    public void testPositiveButtonWhenAccountIsNotManagedAndRemoveLocalDataNotChecked() {
        mockAllowDeletingBrowserHistoryPref(true);

        AlertDialog alertDialog = showSignOutAlertDialog();
        assertNotNull(alertDialog.findViewById(R.id.remove_local_data));
        alertDialog.getButton(AlertDialog.BUTTON_POSITIVE).performClick();
        verify(mSigninMetricsUtilsNativeMock)
                .logProfileAccountManagementMenu(ProfileAccountManagementMetrics.SIGNOUT_SIGNOUT,
                        GAIAServiceType.GAIA_SERVICE_TYPE_NONE);
        verify(mSignOutDialogListenerMock).onSignOutClicked(false);
    }

    @Test
    public void testPositiveButtonWhenAccountIsNotManagedAndRemoveLocalDataChecked() {
        mockAllowDeletingBrowserHistoryPref(true);

        AlertDialog alertDialog = showSignOutAlertDialog();
        alertDialog.findViewById(R.id.remove_local_data).performClick();
        alertDialog.getButton(AlertDialog.BUTTON_POSITIVE).performClick();
        verify(mSigninMetricsUtilsNativeMock)
                .logProfileAccountManagementMenu(ProfileAccountManagementMetrics.SIGNOUT_SIGNOUT,
                        GAIAServiceType.GAIA_SERVICE_TYPE_NONE);
        verify(mSignOutDialogListenerMock).onSignOutClicked(true);
    }

    @Test
    public void testNegativeButtonWhenAccountIsManaged() {
        when(mSigninManagerMock.getManagementDomain()).thenReturn(TEST_DOMAIN);
        AlertDialog alertDialog = showSignOutAlertDialog();
        alertDialog.getButton(AlertDialog.BUTTON_NEGATIVE).performClick();
        verify(mSignOutDialogListenerMock, never()).onSignOutClicked(anyBoolean());
        verify(mSigninMetricsUtilsNativeMock)
                .logProfileAccountManagementMenu(ProfileAccountManagementMetrics.SIGNOUT_CANCEL,
                        GAIAServiceType.GAIA_SERVICE_TYPE_NONE);
    }

    @Test
    public void testNegativeButtonWhenAccountIsNotManaged() {
        mockAllowDeletingBrowserHistoryPref(true);

        AlertDialog alertDialog = showSignOutAlertDialog();
        alertDialog.getButton(AlertDialog.BUTTON_NEGATIVE).performClick();
        verify(mSignOutDialogListenerMock, never()).onSignOutClicked(anyBoolean());
        verify(mSigninMetricsUtilsNativeMock)
                .logProfileAccountManagementMenu(ProfileAccountManagementMetrics.SIGNOUT_CANCEL,
                        GAIAServiceType.GAIA_SERVICE_TYPE_NONE);
    }

    @Test
    public void testEventLoggedWhenDialogDismissed() {
        mockAllowDeletingBrowserHistoryPref(true);

        AlertDialog alertDialog = showSignOutAlertDialog();
        alertDialog.dismiss();
        verify(mSignOutDialogListenerMock, never()).onSignOutClicked(anyBoolean());
        verify(mSigninMetricsUtilsNativeMock)
                .logProfileAccountManagementMenu(ProfileAccountManagementMetrics.SIGNOUT_CANCEL,
                        GAIAServiceType.GAIA_SERVICE_TYPE_NONE);
    }

    private AlertDialog showSignOutAlertDialog() {
        mSignOutDialog.show(mFragmentManager, null);
        return (AlertDialog) ShadowAlertDialog.getLatestDialog();
    }

    private void mockAllowDeletingBrowserHistoryPref(boolean value) {
        when(mUserPrefsNatives.get(mProfile)).thenReturn(mPrefService);
        when(mPrefService.getBoolean(Pref.ALLOW_DELETING_BROWSER_HISTORY)).thenReturn(value);
    }
}
