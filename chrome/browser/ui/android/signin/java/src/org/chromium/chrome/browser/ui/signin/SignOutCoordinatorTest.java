// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.annotation.Nullable;
import androidx.fragment.app.FragmentManager;
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

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridge;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridgeJni;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SignoutReason;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.BlankUiTestActivity;

/** Instrumentation tests for {@link SignOutDialogCoordinator}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
public class SignOutCoordinatorTest {
    @Rule
    public final BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Rule public final JniMocker mocker = new JniMocker();

    @Rule public final Features.JUnitProcessor mFeaturesRule = new Features.JUnitProcessor();

    @Mock private Profile mProfile;
    @Mock private FragmentManager mFragmentManager;
    @Mock private IdentityServicesProvider mIdentityServicesProviderMock;
    @Mock private SigninManager mSigninManagerMock;
    @Mock private IdentityManager mIdentityManagerMock;
    @Mock private PasswordManagerUtilBridge.Natives mPasswordManagerUtilBridgeNativeMock;
    @Mock private UserPrefs.Natives mUserPrefsNatives;
    @Mock private PrefService mPrefService;
    @Mock private Runnable mOnSignOut;

    private SnackbarManager mSnackbarManager;

    @Before
    public void setUp() {
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProviderMock);
        doReturn(mIdentityManagerMock)
                .when(mIdentityServicesProviderMock)
                .getIdentityManager(mProfile);
        doReturn(true).when(mIdentityManagerMock).hasPrimaryAccount(ConsentLevel.SIGNIN);
        doReturn(mSigninManagerMock).when(mIdentityServicesProviderMock).getSigninManager(mProfile);

        mActivityTestRule.launchActivity(null);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSnackbarManager =
                            new SnackbarManager(
                                    mActivityTestRule.getActivity(),
                                    mActivityTestRule
                                            .getActivity()
                                            .findViewById(android.R.id.content),
                                    null);
                });
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void testLegacyDialog_replaceSyncPromosFeatureDisabled() {
        mocker.mock(PasswordManagerUtilBridgeJni.TEST_HOOKS, mPasswordManagerUtilBridgeNativeMock);
        mocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsNatives);
        when(mUserPrefsNatives.get(mProfile)).thenReturn(mPrefService);
        when(mPrefService.getBoolean(Pref.ALLOW_DELETING_BROWSER_HISTORY)).thenReturn(true);

        startSignOutFlow(SignoutReason.USER_CLICKED_SIGNOUT_SETTINGS, null);

        onView(withText(R.string.signout_title)).inRoot(isDialog()).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testLegacyDialog_hasSyncingAccount() {
        mocker.mock(PasswordManagerUtilBridgeJni.TEST_HOOKS, mPasswordManagerUtilBridgeNativeMock);
        mocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsNatives);
        when(mUserPrefsNatives.get(mProfile)).thenReturn(mPrefService);
        when(mPrefService.getBoolean(Pref.ALLOW_DELETING_BROWSER_HISTORY)).thenReturn(true);
        doReturn(true).when(mIdentityManagerMock).hasPrimaryAccount(ConsentLevel.SYNC);

        startSignOutFlow(SignoutReason.USER_CLICKED_SIGNOUT_SETTINGS, null);

        onView(withText(R.string.turn_off_sync_and_signout_title))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testSnackbar_nullOnSignoutCallback() {
        @SignoutReason int signOutReason = SignoutReason.USER_CLICKED_SIGNOUT_SETTINGS;
        doReturn(true).when(mSigninManagerMock).isSignOutAllowed();
        doAnswer(
                        args -> {
                            args.getArgument(0, Runnable.class).run();
                            return null;
                        })
                .when(mSigninManagerMock)
                .runAfterOperationInProgress(any(Runnable.class));
        doAnswer(
                        args -> {
                            args.getArgument(1, SigninManager.SignOutCallback.class)
                                    .signOutComplete();
                            return null;
                        })
                .when(mSigninManagerMock)
                .signOut(eq(signOutReason), any(SigninManager.SignOutCallback.class), eq(true));

        startSignOutFlow(signOutReason, null);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertTrue(mSnackbarManager.isShowing());
                    Snackbar currentSnackbar = mSnackbarManager.getCurrentSnackbarForTesting();
                    Assert.assertEquals(
                            currentSnackbar.getIdentifierForTesting(), Snackbar.UMA_SIGN_OUT);
                    Assert.assertEquals(
                            currentSnackbar.getTextForTesting(),
                            mActivityTestRule
                                    .getActivity()
                                    .getString(R.string.sign_out_snackbar_message));
                });
    }

    @Test
    @MediumTest
    public void testSnackbar_nonNullOnSignoutCallback() {
        @SignoutReason int signOutReason = SignoutReason.USER_CLICKED_SIGNOUT_SETTINGS;
        doReturn(true).when(mSigninManagerMock).isSignOutAllowed();
        doAnswer(
                        args -> {
                            args.getArgument(0, Runnable.class).run();
                            return null;
                        })
                .when(mSigninManagerMock)
                .runAfterOperationInProgress(any(Runnable.class));
        doAnswer(
                        args -> {
                            args.getArgument(1, SigninManager.SignOutCallback.class)
                                    .signOutComplete();
                            return null;
                        })
                .when(mSigninManagerMock)
                .signOut(eq(signOutReason), any(SigninManager.SignOutCallback.class), eq(true));

        startSignOutFlow(signOutReason, mOnSignOut);

        verify(mOnSignOut).run();
    }

    private void startSignOutFlow(@SignoutReason int signoutReason, @Nullable Runnable onSignOut) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SignOutCoordinator.startSignOutFlow(
                            mActivityTestRule.getActivity(),
                            mProfile,
                            mFragmentManager,
                            mActivityTestRule.getActivity().getModalDialogManager(),
                            mSnackbarManager,
                            signoutReason,
                            onSignOut);
                });
    }
}
