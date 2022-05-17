// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.pressBack;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isRoot;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.support.test.InstrumentationRegistry;

import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileAccountManagementMetrics;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtilsJni;
import org.chromium.chrome.browser.ui.signin.SignOutDialogFragment.SignOutDialogListener;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.GAIAServiceType;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.ui.test.util.BlankUiTestActivity;

/** Instrumentation tests for {@link SignOutDialogFragment}. */
@RunWith(ChromeJUnit4ClassRunner.class)
public class SignOutDialogTest {
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

    @Rule
    public final BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

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
        mActivityTestRule.launchActivity(null);
        setUpSignOutDialog();
    }

    private void setUpSignOutDialog() {
        mFragmentManager = mActivityTestRule.getActivity().getSupportFragmentManager();
        mTargetFragment.setListener(mSignOutDialogListenerMock);
        mFragmentManager.beginTransaction().add(mTargetFragment, null).commit();
        mSignOutDialog =
                SignOutDialogFragment.create(SignOutDialogFragment.ActionType.CLEAR_PRIMARY_ACCOUNT,
                        GAIAServiceType.GAIA_SERVICE_TYPE_NONE);
        mSignOutDialog.setTargetFragment(mTargetFragment, 0);
    }

    @Test
    @MediumTest
    public void testMessageWhenAccountIsManaged() {
        when(mSigninManagerMock.getManagementDomain()).thenReturn(TEST_DOMAIN);

        showSignOutAlertDialog();

        onView(withText(mSignOutDialog.getString(
                       R.string.signout_managed_account_message, TEST_DOMAIN)))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testNoDataWipeCheckboxWhenAccountIsManaged() {
        when(mSigninManagerMock.getManagementDomain()).thenReturn(TEST_DOMAIN);

        showSignOutAlertDialog();

        onView(withId(R.id.remove_local_data)).inRoot(isDialog()).check(doesNotExist());
    }

    @Test
    @MediumTest
    public void testPositiveButtonWhenAccountIsManaged() {
        when(mSigninManagerMock.getManagementDomain()).thenReturn(TEST_DOMAIN);
        showSignOutAlertDialog();

        onView(withText(R.string.continue_button)).inRoot(isDialog()).perform(click());

        verify(mSigninMetricsUtilsNativeMock)
                .logProfileAccountManagementMenu(ProfileAccountManagementMetrics.SIGNOUT_SIGNOUT,
                        GAIAServiceType.GAIA_SERVICE_TYPE_NONE);
        verify(mSignOutDialogListenerMock).onSignOutClicked(false);
    }

    @Test
    @MediumTest
    public void testNoDataWipeCheckboxWhenAccountIsNotManagedAndHistoryDeletionNotAllowed() {
        mockAllowDeletingBrowserHistoryPref(false);

        showSignOutAlertDialog();

        onView(withId(R.id.remove_local_data)).inRoot(isDialog()).check(doesNotExist());
    }

    @Test
    @MediumTest
    public void testPositiveButtonWhenAccountIsNotManagedAndHistoryDeletionNotAllowed() {
        mockAllowDeletingBrowserHistoryPref(false);
        showSignOutAlertDialog();

        onView(withText(R.string.continue_button)).inRoot(isDialog()).perform(click());

        verify(mSigninMetricsUtilsNativeMock)
                .logProfileAccountManagementMenu(ProfileAccountManagementMetrics.SIGNOUT_SIGNOUT,
                        GAIAServiceType.GAIA_SERVICE_TYPE_NONE);
        verify(mSignOutDialogListenerMock).onSignOutClicked(false);
    }

    @Test
    @MediumTest
    public void testPositiveButtonWhenAccountIsNotManagedAndRemoveLocalDataNotChecked() {
        mockAllowDeletingBrowserHistoryPref(true);
        showSignOutAlertDialog();
        onView(withId(R.id.remove_local_data)).inRoot(isDialog()).check(matches(isDisplayed()));

        onView(withText(R.string.continue_button)).inRoot(isDialog()).perform(click());

        verify(mSigninMetricsUtilsNativeMock)
                .logProfileAccountManagementMenu(ProfileAccountManagementMetrics.SIGNOUT_SIGNOUT,
                        GAIAServiceType.GAIA_SERVICE_TYPE_NONE);
        verify(mSignOutDialogListenerMock).onSignOutClicked(false);
    }

    @Test
    @MediumTest
    public void testPositiveButtonWhenAccountIsNotManagedAndRemoveLocalDataChecked() {
        mockAllowDeletingBrowserHistoryPref(true);
        showSignOutAlertDialog();

        onView(withId(R.id.remove_local_data)).inRoot(isDialog()).perform(click());
        onView(withText(R.string.continue_button)).inRoot(isDialog()).perform(click());

        verify(mSigninMetricsUtilsNativeMock)
                .logProfileAccountManagementMenu(ProfileAccountManagementMetrics.SIGNOUT_SIGNOUT,
                        GAIAServiceType.GAIA_SERVICE_TYPE_NONE);
        verify(mSignOutDialogListenerMock).onSignOutClicked(true);
    }

    @Test
    @MediumTest
    public void testNegativeButtonWhenAccountIsManaged() {
        when(mSigninManagerMock.getManagementDomain()).thenReturn(TEST_DOMAIN);
        showSignOutAlertDialog();

        onView(withText(R.string.cancel)).inRoot(isDialog()).perform(click());

        verify(mSignOutDialogListenerMock, never()).onSignOutClicked(anyBoolean());
        verify(mSigninMetricsUtilsNativeMock)
                .logProfileAccountManagementMenu(ProfileAccountManagementMetrics.SIGNOUT_CANCEL,
                        GAIAServiceType.GAIA_SERVICE_TYPE_NONE);
    }

    @Test
    @MediumTest
    public void testNegativeButtonWhenAccountIsNotManaged() {
        mockAllowDeletingBrowserHistoryPref(true);
        showSignOutAlertDialog();

        onView(withText(R.string.cancel)).inRoot(isDialog()).perform(click());

        verify(mSignOutDialogListenerMock, never()).onSignOutClicked(anyBoolean());
        verify(mSigninMetricsUtilsNativeMock)
                .logProfileAccountManagementMenu(ProfileAccountManagementMetrics.SIGNOUT_CANCEL,
                        GAIAServiceType.GAIA_SERVICE_TYPE_NONE);
    }

    @Test
    @MediumTest
    public void testEventLoggedWhenDialogDismissed() {
        mockAllowDeletingBrowserHistoryPref(true);
        showSignOutAlertDialog();

        onView(isRoot()).perform(pressBack());

        verify(mSignOutDialogListenerMock, never()).onSignOutClicked(anyBoolean());
        verify(mSigninMetricsUtilsNativeMock)
                .logProfileAccountManagementMenu(ProfileAccountManagementMetrics.SIGNOUT_CANCEL,
                        GAIAServiceType.GAIA_SERVICE_TYPE_NONE);
    }

    private void showSignOutAlertDialog() {
        mSignOutDialog.show(mFragmentManager, null);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        // This enum is recorded whenever the sign out dialog is shown.
        verify(mSigninMetricsUtilsNativeMock)
                .logProfileAccountManagementMenu(ProfileAccountManagementMetrics.TOGGLE_SIGNOUT,
                        GAIAServiceType.GAIA_SERVICE_TYPE_NONE);
    }

    private void mockAllowDeletingBrowserHistoryPref(boolean value) {
        when(mUserPrefsNatives.get(mProfile)).thenReturn(mPrefService);
        when(mPrefService.getBoolean(Pref.ALLOW_DELETING_BROWSER_HISTORY)).thenReturn(value);
    }
}
