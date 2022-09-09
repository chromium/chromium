// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.pressBack;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isRoot;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.not;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

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
import org.chromium.chrome.browser.ui.signin.SignOutDialogCoordinator.ActionType;
import org.chromium.chrome.browser.ui.signin.SignOutDialogCoordinator.Listener;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.GAIAServiceType;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.BlankUiTestActivity;

/** Instrumentation tests for {@link SignOutDialogCoordinator}. */
@RunWith(ChromeJUnit4ClassRunner.class)
public class SignOutDialogTest {
    private static final String TEST_DOMAIN = "test.domain.example.com";

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
    private IdentityManager mIdentityManagerMock;

    @Mock
    private Profile mProfile;

    @Mock
    private UserPrefs.Natives mUserPrefsNatives;

    @Mock
    private PrefService mPrefService;

    @Mock
    private Listener mListenerMock;

    @Before
    public void setUp() {
        mocker.mock(SigninMetricsUtilsJni.TEST_HOOKS, mSigninMetricsUtilsNativeMock);
        mocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsNatives);
        IdentityServicesProvider.setInstanceForTests(mock(IdentityServicesProvider.class));
        Profile.setLastUsedProfileForTesting(mProfile);
        when(IdentityServicesProvider.get().getSigninManager(any())).thenReturn(mSigninManagerMock);
        when(IdentityServicesProvider.get().getIdentityManager(any()))
                .thenReturn(mIdentityManagerMock);
        when(mIdentityManagerMock.hasPrimaryAccount(anyInt())).thenReturn(true);
        mActivityTestRule.launchActivity(null);
    }

    @Test
    @MediumTest
    public void testDialogForNonSyncingAccount() {
        mockAllowDeletingBrowserHistoryPref(true);
        when(mIdentityManagerMock.hasPrimaryAccount(ConsentLevel.SYNC)).thenReturn(false);

        showSignOutDialog();

        onView(withText(R.string.signout_title)).inRoot(isDialog()).check(matches(isDisplayed()));
        onView(withText(R.string.signout_message)).inRoot(isDialog()).check(matches(isDisplayed()));
        onView(withId(R.id.remove_local_data))
                .inRoot(isDialog())
                .check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    public void testDialogForNonSyncingManagedAccount() {
        mockAllowDeletingBrowserHistoryPref(true);
        when(mSigninManagerMock.getManagementDomain()).thenReturn(TEST_DOMAIN);
        when(mIdentityManagerMock.hasPrimaryAccount(ConsentLevel.SYNC)).thenReturn(false);

        showSignOutDialog();

        onView(withText(R.string.signout_title)).inRoot(isDialog()).check(matches(isDisplayed()));
        onView(withText(R.string.signout_message)).inRoot(isDialog()).check(matches(isDisplayed()));
        onView(withId(R.id.remove_local_data))
                .inRoot(isDialog())
                .check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    public void testMessageWhenAccountIsManaged() {
        mockAllowDeletingBrowserHistoryPref(true);
        when(mSigninManagerMock.getManagementDomain()).thenReturn(TEST_DOMAIN);

        showSignOutDialog();

        onView(withText(mActivityTestRule.getActivity().getString(
                       R.string.signout_managed_account_message, TEST_DOMAIN)))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testNoDataWipeCheckboxWhenAccountIsManaged() {
        mockAllowDeletingBrowserHistoryPref(true);
        when(mSigninManagerMock.getManagementDomain()).thenReturn(TEST_DOMAIN);

        showSignOutDialog();

        onView(withId(R.id.remove_local_data))
                .inRoot(isDialog())
                .check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    public void testNoDataWipeCheckboxWhenAccountIsManagedAndHistoryDeletionNotAllowed() {
        mockAllowDeletingBrowserHistoryPref(false);
        when(mSigninManagerMock.getManagementDomain()).thenReturn(TEST_DOMAIN);

        showSignOutDialog();

        onView(withId(R.id.remove_local_data))
                .inRoot(isDialog())
                .check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    public void testPositiveButtonWhenAccountIsManaged() {
        mockAllowDeletingBrowserHistoryPref(true);
        when(mSigninManagerMock.getManagementDomain()).thenReturn(TEST_DOMAIN);
        showSignOutDialog();

        onView(withText(R.string.continue_button)).inRoot(isDialog()).perform(click());

        verify(mSigninMetricsUtilsNativeMock)
                .logProfileAccountManagementMenu(ProfileAccountManagementMetrics.SIGNOUT_SIGNOUT,
                        GAIAServiceType.GAIA_SERVICE_TYPE_NONE);
        verify(mListenerMock).onSignOutClicked(false);
    }

    @Test
    @MediumTest
    public void testNoDataWipeCheckboxWhenAccountIsNotManagedAndHistoryDeletionNotAllowed() {
        mockAllowDeletingBrowserHistoryPref(false);

        showSignOutDialog();

        onView(withId(R.id.remove_local_data))
                .inRoot(isDialog())
                .check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    public void testPositiveButtonWhenAccountIsNotManagedAndHistoryDeletionNotAllowed() {
        mockAllowDeletingBrowserHistoryPref(false);
        showSignOutDialog();

        onView(withText(R.string.continue_button)).inRoot(isDialog()).perform(click());

        verify(mSigninMetricsUtilsNativeMock)
                .logProfileAccountManagementMenu(ProfileAccountManagementMetrics.SIGNOUT_SIGNOUT,
                        GAIAServiceType.GAIA_SERVICE_TYPE_NONE);
        verify(mListenerMock).onSignOutClicked(false);
    }

    @Test
    @MediumTest
    public void testPositiveButtonWhenAccountIsNotManagedAndRemoveLocalDataNotChecked() {
        mockAllowDeletingBrowserHistoryPref(true);
        showSignOutDialog();
        onView(withId(R.id.remove_local_data)).inRoot(isDialog()).check(matches(isDisplayed()));

        onView(withText(R.string.continue_button)).inRoot(isDialog()).perform(click());

        verify(mSigninMetricsUtilsNativeMock)
                .logProfileAccountManagementMenu(ProfileAccountManagementMetrics.SIGNOUT_SIGNOUT,
                        GAIAServiceType.GAIA_SERVICE_TYPE_NONE);
        verify(mListenerMock).onSignOutClicked(false);
    }

    @Test
    @MediumTest
    public void testPositiveButtonWhenAccountIsNotManagedAndRemoveLocalDataChecked() {
        mockAllowDeletingBrowserHistoryPref(true);
        showSignOutDialog();

        onView(withId(R.id.remove_local_data)).inRoot(isDialog()).perform(click());
        onView(withText(R.string.continue_button)).inRoot(isDialog()).perform(click());

        verify(mSigninMetricsUtilsNativeMock)
                .logProfileAccountManagementMenu(ProfileAccountManagementMetrics.SIGNOUT_SIGNOUT,
                        GAIAServiceType.GAIA_SERVICE_TYPE_NONE);
        verify(mListenerMock).onSignOutClicked(true);
    }

    @Test
    @MediumTest
    public void testNegativeButtonWhenAccountIsManaged() {
        mockAllowDeletingBrowserHistoryPref(true);
        when(mSigninManagerMock.getManagementDomain()).thenReturn(TEST_DOMAIN);
        showSignOutDialog();

        onView(withText(R.string.cancel)).inRoot(isDialog()).perform(click());

        verify(mListenerMock, never()).onSignOutClicked(anyBoolean());
        verify(mSigninMetricsUtilsNativeMock)
                .logProfileAccountManagementMenu(ProfileAccountManagementMetrics.SIGNOUT_CANCEL,
                        GAIAServiceType.GAIA_SERVICE_TYPE_NONE);
    }

    @Test
    @MediumTest
    public void testNegativeButtonWhenAccountIsNotManaged() {
        mockAllowDeletingBrowserHistoryPref(true);
        showSignOutDialog();

        onView(withText(R.string.cancel)).inRoot(isDialog()).perform(click());

        verify(mListenerMock, never()).onSignOutClicked(anyBoolean());
        verify(mSigninMetricsUtilsNativeMock)
                .logProfileAccountManagementMenu(ProfileAccountManagementMetrics.SIGNOUT_CANCEL,
                        GAIAServiceType.GAIA_SERVICE_TYPE_NONE);
    }

    @Test
    @MediumTest
    public void testEventLoggedWhenDialogDismissed() {
        mockAllowDeletingBrowserHistoryPref(true);
        showSignOutDialog();

        onView(isRoot()).perform(pressBack());

        verify(mListenerMock, never()).onSignOutClicked(anyBoolean());
        verify(mSigninMetricsUtilsNativeMock)
                .logProfileAccountManagementMenu(ProfileAccountManagementMetrics.SIGNOUT_CANCEL,
                        GAIAServiceType.GAIA_SERVICE_TYPE_NONE);
    }

    private void showSignOutDialog() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            SignOutDialogCoordinator.show(mActivityTestRule.getActivity(),
                    mActivityTestRule.getActivity().getModalDialogManager(), mListenerMock,
                    ActionType.CLEAR_PRIMARY_ACCOUNT, GAIAServiceType.GAIA_SERVICE_TYPE_NONE);
        });
        verify(mSigninMetricsUtilsNativeMock)
                .logProfileAccountManagementMenu(ProfileAccountManagementMetrics.TOGGLE_SIGNOUT,
                        GAIAServiceType.GAIA_SERVICE_TYPE_NONE);
    }

    private void mockAllowDeletingBrowserHistoryPref(boolean value) {
        when(mUserPrefsNatives.get(mProfile)).thenReturn(mPrefService);
        when(mPrefService.getBoolean(Pref.ALLOW_DELETING_BROWSER_HISTORY)).thenReturn(value);
    }
}
