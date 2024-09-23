// Copyright 2019 The Chromium Authors
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

import static org.hamcrest.Matchers.not;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
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
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridge;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridgeJni;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninManager.SignOutCallback;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SignoutReason;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.ui.test.util.BlankUiTestActivity;

/** Instrumentation tests for {@link SignOutDialogCoordinator}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class SignOutDialogTest {
    private static final String TEST_DOMAIN = "test.domain.example.com";

    @Rule public final JniMocker mocker = new JniMocker();

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Rule
    public final BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Mock private PasswordManagerUtilBridge.Natives mPasswordManagerUtilBridgeNativeMock;

    @Mock private IdentityServicesProvider mIdentityServicesProviderMock;

    @Mock private SigninManager mSigninManagerMock;

    @Mock private IdentityManager mIdentityManagerMock;

    @Mock private Profile mProfile;

    @Mock private UserPrefs.Natives mUserPrefsNatives;

    @Mock private PrefService mPrefService;

    @Mock private Runnable mOnSignOut;

    @Before
    public void setUp() {
        mActivityTestRule.launchActivity(null);
    }

    @Test
    @MediumTest
    public void testRegularAccountCanNotRevokeSyncConsent() {
        when(mProfile.isChild()).thenReturn(false);
        // ThreadUtils.runOnUiThreadBlocking() catches the IllegalArgumentException and throws it
        // wrapped inside a RuntimeException.
        Assert.assertThrows(
                RuntimeException.class,
                () -> showSignOutDialog(SignoutReason.USER_CLICKED_REVOKE_SYNC_CONSENT_SETTINGS));
    }

    @Test
    @MediumTest
    public void testChildAccountCanOnlyRevokeSyncConsent() {
        when(mProfile.isChild()).thenReturn(true);
        // ThreadUtils.runOnUiThreadBlocking() catches the IllegalArgumentException and throws it
        // wrapped inside a RuntimeException.
        Assert.assertThrows(
                RuntimeException.class,
                () -> showSignOutDialog(SignoutReason.USER_CLICKED_SIGNOUT_SETTINGS));
    }

    @Test
    @MediumTest
    public void testDialogForNonSyncingAccount() {
        setUpMocks();
        mockAllowDeletingBrowserHistoryPref(true);
        when(mIdentityManagerMock.hasPrimaryAccount(ConsentLevel.SYNC)).thenReturn(false);

        showSignOutDialog(SignoutReason.USER_CLICKED_SIGNOUT_SETTINGS);

        onView(withText(R.string.signout_title)).inRoot(isDialog()).check(matches(isDisplayed()));
        onView(withText(R.string.signout_message)).inRoot(isDialog()).check(matches(isDisplayed()));
        onView(withId(R.id.remove_local_data))
                .inRoot(isDialog())
                .check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    public void testDialogForNonSyncingManagedAccount() {
        setUpMocks();
        mockAllowDeletingBrowserHistoryPref(true);
        when(mSigninManagerMock.getManagementDomain()).thenReturn(TEST_DOMAIN);
        when(mIdentityManagerMock.hasPrimaryAccount(ConsentLevel.SYNC)).thenReturn(false);

        showSignOutDialog(SignoutReason.USER_CLICKED_SIGNOUT_SETTINGS);

        onView(withText(R.string.signout_title)).inRoot(isDialog()).check(matches(isDisplayed()));
        onView(withText(R.string.signout_message)).inRoot(isDialog()).check(matches(isDisplayed()));
        onView(withId(R.id.remove_local_data))
                .inRoot(isDialog())
                .check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    public void testMessageWhenAccountIsManaged() {
        setUpMocks();
        mockAllowDeletingBrowserHistoryPref(true);
        when(mSigninManagerMock.getManagementDomain()).thenReturn(TEST_DOMAIN);

        showSignOutDialog(SignoutReason.USER_CLICKED_SIGNOUT_SETTINGS);

        onView(
                        withText(
                                mActivityTestRule
                                        .getActivity()
                                        .getString(
                                                R.string.signout_managed_account_message,
                                                TEST_DOMAIN)))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testNoDataWipeCheckboxWhenAccountIsManaged() {
        setUpMocks();
        mockAllowDeletingBrowserHistoryPref(true);
        when(mSigninManagerMock.getManagementDomain()).thenReturn(TEST_DOMAIN);

        showSignOutDialog(SignoutReason.USER_CLICKED_SIGNOUT_SETTINGS);

        onView(withId(R.id.remove_local_data))
                .inRoot(isDialog())
                .check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    public void testNoDataWipeCheckboxWhenAccountIsManagedAndHistoryDeletionNotAllowed() {
        setUpMocks();
        mockAllowDeletingBrowserHistoryPref(false);
        when(mSigninManagerMock.getManagementDomain()).thenReturn(TEST_DOMAIN);

        showSignOutDialog(SignoutReason.USER_CLICKED_SIGNOUT_SETTINGS);

        onView(withId(R.id.remove_local_data))
                .inRoot(isDialog())
                .check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    public void testNoDataWipeCheckboxWhenAccountIsNotManagedAndHistoryDeletionNotAllowed() {
        setUpMocks();
        mockAllowDeletingBrowserHistoryPref(false);

        showSignOutDialog(SignoutReason.USER_CLICKED_SIGNOUT_SETTINGS);

        onView(withId(R.id.remove_local_data))
                .inRoot(isDialog())
                .check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    public void testFooterWhenAccountIsNotManaged_UPMDisabled() {
        setUpMocks();
        mockAllowDeletingBrowserHistoryPref(true);
        when(mPasswordManagerUtilBridgeNativeMock.usesSplitStoresAndUPMForLocal(mPrefService))
                .thenReturn(false);

        showSignOutDialog(SignoutReason.USER_CLICKED_SIGNOUT_SETTINGS);

        onView(withText(R.string.turn_off_sync_and_signout_message))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testFooterWhenAccountIsNotManaged_UPMEnabled() {
        setUpMocks();
        mockAllowDeletingBrowserHistoryPref(true);
        when(mPasswordManagerUtilBridgeNativeMock.usesSplitStoresAndUPMForLocal(mPrefService))
                .thenReturn(true);

        showSignOutDialog(SignoutReason.USER_CLICKED_SIGNOUT_SETTINGS);

        onView(withText(R.string.turn_off_sync_and_signout_message_without_passwords))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testPositiveButtonWhenAccountIsManaged() {
        setUpMocks();
        mockAllowDeletingBrowserHistoryPref(true);
        when(mSigninManagerMock.getManagementDomain()).thenReturn(TEST_DOMAIN);
        when(mSigninManagerMock.isSignOutAllowed()).thenReturn(true);
        doAnswer(
                        args -> {
                            args.getArgument(0, Runnable.class).run();
                            return null;
                        })
                .when(mSigninManagerMock)
                .runAfterOperationInProgress(any(Runnable.class));
        showSignOutDialog(SignoutReason.USER_CLICKED_SIGNOUT_SETTINGS);

        onView(withText(R.string.continue_button)).inRoot(isDialog()).perform(click());
        onView(withId(android.R.id.message)).check(doesNotExist());

        verify(mSigninManagerMock)
                .signOut(
                        eq(SignoutReason.USER_CLICKED_SIGNOUT_SETTINGS),
                        any(SignOutCallback.class),
                        eq(false));
    }

    @Test
    @MediumTest
    public void testPositiveButtonWhenAccountIsNotManagedAndHistoryDeletionNotAllowed() {
        setUpMocks();
        mockAllowDeletingBrowserHistoryPref(false);
        when(mSigninManagerMock.isSignOutAllowed()).thenReturn(true);
        doAnswer(
                        args -> {
                            args.getArgument(0, Runnable.class).run();
                            return null;
                        })
                .when(mSigninManagerMock)
                .runAfterOperationInProgress(any(Runnable.class));
        showSignOutDialog(SignoutReason.USER_CLICKED_SIGNOUT_SETTINGS);

        onView(withText(R.string.continue_button)).inRoot(isDialog()).perform(click());
        onView(withId(android.R.id.message)).check(doesNotExist());

        verify(mSigninManagerMock)
                .signOut(
                        eq(SignoutReason.USER_CLICKED_SIGNOUT_SETTINGS),
                        any(SignOutCallback.class),
                        eq(false));
    }

    @Test
    @MediumTest
    public void testPositiveButtonWhenAccountIsNotManagedAndRemoveLocalDataNotChecked() {
        setUpMocks();
        mockAllowDeletingBrowserHistoryPref(true);
        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Signin.UserRequestedWipeDataOnSignout", false);
        when(mSigninManagerMock.isSignOutAllowed()).thenReturn(true);
        doAnswer(
                        args -> {
                            args.getArgument(0, Runnable.class).run();
                            return null;
                        })
                .when(mSigninManagerMock)
                .runAfterOperationInProgress(any(Runnable.class));
        showSignOutDialog(SignoutReason.USER_CLICKED_SIGNOUT_SETTINGS);
        onView(withId(R.id.remove_local_data)).inRoot(isDialog()).check(matches(isDisplayed()));

        onView(withText(R.string.continue_button)).inRoot(isDialog()).perform(click());
        onView(withId(android.R.id.message)).check(doesNotExist());

        histogramWatcher.assertExpected();
        verify(mSigninManagerMock)
                .signOut(
                        eq(SignoutReason.USER_CLICKED_SIGNOUT_SETTINGS),
                        any(SignOutCallback.class),
                        eq(false));
    }

    @Test
    @MediumTest
    public void testPositiveButtonWhenAccountIsNotManagedAndRemoveLocalDataChecked() {
        setUpMocks();
        mockAllowDeletingBrowserHistoryPref(true);
        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Signin.UserRequestedWipeDataOnSignout", true);
        when(mSigninManagerMock.isSignOutAllowed()).thenReturn(true);
        doAnswer(
                        args -> {
                            args.getArgument(0, Runnable.class).run();
                            return null;
                        })
                .when(mSigninManagerMock)
                .runAfterOperationInProgress(any(Runnable.class));
        showSignOutDialog(SignoutReason.USER_CLICKED_SIGNOUT_SETTINGS);

        onView(withId(R.id.remove_local_data)).inRoot(isDialog()).perform(click());
        onView(withText(R.string.continue_button)).inRoot(isDialog()).perform(click());
        onView(withId(android.R.id.message)).check(doesNotExist());

        histogramWatcher.assertExpected();
        verify(mSigninManagerMock)
                .signOut(
                        eq(SignoutReason.USER_CLICKED_SIGNOUT_SETTINGS),
                        any(SignOutCallback.class),
                        eq(true));
    }

    @Test
    @MediumTest
    public void testPositiveButtonForChildAccount() {
        setUpMocks();
        mockAllowDeletingBrowserHistoryPref(true);
        when(mProfile.isChild()).thenReturn(true);
        doAnswer(
                        args -> {
                            args.getArgument(0, Runnable.class).run();
                            return null;
                        })
                .when(mSigninManagerMock)
                .runAfterOperationInProgress(any(Runnable.class));
        showSignOutDialog(SignoutReason.USER_CLICKED_REVOKE_SYNC_CONSENT_SETTINGS);

        onView(withText(R.string.continue_button)).inRoot(isDialog()).perform(click());
        onView(withId(android.R.id.message)).check(doesNotExist());

        verify(mSigninManagerMock, times(0)).isSignOutAllowed();
        verify(mSigninManagerMock)
                .revokeSyncConsent(
                        eq(SignoutReason.USER_CLICKED_REVOKE_SYNC_CONSENT_SETTINGS),
                        any(SignOutCallback.class),
                        eq(false));
    }

    @Test
    @MediumTest
    public void testPositiveButtonWhenSignOutNotAllowed() {
        setUpMocks();
        mockAllowDeletingBrowserHistoryPref(true);
        when(mSigninManagerMock.isSignOutAllowed()).thenReturn(false);
        doAnswer(
                        args -> {
                            args.getArgument(0, Runnable.class).run();
                            return null;
                        })
                .when(mSigninManagerMock)
                .runAfterOperationInProgress(any(Runnable.class));
        showSignOutDialog(SignoutReason.USER_CLICKED_SIGNOUT_SETTINGS);

        onView(withText(R.string.continue_button)).inRoot(isDialog()).perform(click());
        onView(withId(android.R.id.message)).check(doesNotExist());

        verify(mSigninManagerMock, never())
                .signOut(anyInt(), any(SignOutCallback.class), anyBoolean());
    }

    @Test
    @MediumTest
    public void testPositiveButtonCallsOnSignOutClicked() {
        setUpMocks();
        mockAllowDeletingBrowserHistoryPref(true);
        when(mSigninManagerMock.isSignOutAllowed()).thenReturn(true);
        doAnswer(
                        args -> {
                            args.getArgument(0, Runnable.class).run();
                            return null;
                        })
                .when(mSigninManagerMock)
                .runAfterOperationInProgress(any(Runnable.class));
        doAnswer(
                        args -> {
                            args.getArgument(1, SignOutCallback.class).signOutComplete();
                            return null;
                        })
                .when(mSigninManagerMock)
                .signOut(anyInt(), any(SignOutCallback.class), anyBoolean());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SignOutDialogCoordinator.show(
                            mActivityTestRule.getActivity(),
                            mProfile,
                            mActivityTestRule.getActivity().getSupportFragmentManager(),
                            mActivityTestRule.getActivity().getModalDialogManager(),
                            SignoutReason.USER_CLICKED_SIGNOUT_SETTINGS,
                            mOnSignOut);
                });

        onView(withText(R.string.continue_button)).inRoot(isDialog()).perform(click());
        onView(withId(android.R.id.message)).check(doesNotExist());

        verify(mOnSignOut).run();
    }

    @Test
    @MediumTest
    public void testNegativeButtonWhenAccountIsManaged() {
        setUpMocks();
        mockAllowDeletingBrowserHistoryPref(true);
        when(mSigninManagerMock.getManagementDomain()).thenReturn(TEST_DOMAIN);
        showSignOutDialog(SignoutReason.USER_CLICKED_SIGNOUT_SETTINGS);

        onView(withText(R.string.cancel)).inRoot(isDialog()).perform(click());
        onView(withId(android.R.id.message)).check(doesNotExist());

        verify(mSigninManagerMock, never())
                .signOut(anyInt(), any(SignOutCallback.class), anyBoolean());
    }

    @Test
    @MediumTest
    public void testNegativeButtonWhenAccountIsNotManaged() {
        setUpMocks();
        mockAllowDeletingBrowserHistoryPref(true);
        showSignOutDialog(SignoutReason.USER_CLICKED_SIGNOUT_SETTINGS);

        onView(withText(R.string.cancel)).inRoot(isDialog()).perform(click());
        onView(withId(android.R.id.message)).check(doesNotExist());

        verify(mSigninManagerMock, never())
                .signOut(anyInt(), any(SignOutCallback.class), anyBoolean());
    }

    @Test
    @MediumTest
    public void testEventLoggedWhenDialogDismissed() {
        setUpMocks();
        mockAllowDeletingBrowserHistoryPref(true);
        showSignOutDialog(SignoutReason.USER_CLICKED_SIGNOUT_SETTINGS);

        onView(isRoot()).perform(pressBack());
        onView(withId(android.R.id.message)).check(doesNotExist());

        verify(mSigninManagerMock, never())
                .signOut(anyInt(), any(SignOutCallback.class), anyBoolean());
    }

    private void setUpMocks() {
        mocker.mock(PasswordManagerUtilBridgeJni.TEST_HOOKS, mPasswordManagerUtilBridgeNativeMock);
        mocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsNatives);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProviderMock);
        when(mIdentityServicesProviderMock.getSigninManager(any())).thenReturn(mSigninManagerMock);
        when(mIdentityServicesProviderMock.getIdentityManager(any()))
                .thenReturn(mIdentityManagerMock);
        when(mIdentityManagerMock.hasPrimaryAccount(anyInt())).thenReturn(true);
        when(mProfile.isChild()).thenReturn(false);
    }

    private void showSignOutDialog(@SignoutReason int signOutReason) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SignOutDialogCoordinator.show(
                            mActivityTestRule.getActivity(),
                            mProfile,
                            mActivityTestRule.getActivity().getSupportFragmentManager(),
                            mActivityTestRule.getActivity().getModalDialogManager(),
                            signOutReason,
                            /* onSignOut= */ null);
                });
        onView(withId(android.R.id.message)).inRoot(isDialog()).check(matches(isDisplayed()));
    }

    private void mockAllowDeletingBrowserHistoryPref(boolean value) {
        when(mUserPrefsNatives.get(mProfile)).thenReturn(mPrefService);
        when(mPrefService.getBoolean(Pref.ALLOW_DELETING_BROWSER_HISTORY)).thenReturn(value);
    }
}
