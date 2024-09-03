// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

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

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
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
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SignoutReason;
import org.chromium.components.sync.DataType;
import org.chromium.components.sync.SyncService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.ui.test.util.BlankUiTestActivity;

import java.util.HashSet;
import java.util.Set;

/** Instrumentation tests for {@link SignOutDialogCoordinator}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@EnableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
public class SignOutCoordinatorTest {
    @Rule
    public final BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Rule public final JniMocker mocker = new JniMocker();

    @Mock private Profile mProfile;
    @Mock private FragmentManager mFragmentManager;
    @Mock private IdentityServicesProvider mIdentityServicesProviderMock;
    @Mock private IdentityManager mIdentityManagerMock;
    @Mock private SigninManager mSigninManagerMock;
    @Mock private SyncService mSyncService;
    @Mock private PasswordManagerUtilBridge.Natives mPasswordManagerUtilBridgeNativeMock;
    @Mock private UserPrefs.Natives mUserPrefsNatives;
    @Mock private PrefService mPrefService;
    @Mock private Runnable mOnSignOut;

    private final Set<Integer> mUnsyncedDataTypes = new HashSet<>();
    private SnackbarManager mSnackbarManager;

    @Before
    public void setUp() {
        LibraryLoader.getInstance().ensureInitialized();
        mActivityTestRule.launchActivity(null);
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void testLegacyDialog_replaceSyncPromosFeatureDisabled() {
        setUpMocks();
        mocker.mock(PasswordManagerUtilBridgeJni.TEST_HOOKS, mPasswordManagerUtilBridgeNativeMock);
        mocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsNatives);
        when(mUserPrefsNatives.get(mProfile)).thenReturn(mPrefService);
        when(mPrefService.getBoolean(Pref.ALLOW_DELETING_BROWSER_HISTORY)).thenReturn(true);

        startSignOutFlow(SignoutReason.USER_CLICKED_SIGNOUT_SETTINGS, mOnSignOut, false);

        onView(withText(R.string.signout_title)).inRoot(isDialog()).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void testLegacyDialogWithRevokeSyncConsentReason_replaceSyncPromosFeatureEnabled() {
        setUpMocks();
        doReturn(true).when(mIdentityManagerMock).hasPrimaryAccount(ConsentLevel.SYNC);
        mocker.mock(PasswordManagerUtilBridgeJni.TEST_HOOKS, mPasswordManagerUtilBridgeNativeMock);
        mocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsNatives);
        when(mUserPrefsNatives.get(mProfile)).thenReturn(mPrefService);
        when(mProfile.isChild()).thenReturn(true);
        when(mPrefService.getBoolean(Pref.ALLOW_DELETING_BROWSER_HISTORY)).thenReturn(true);

        startSignOutFlow(
                SignoutReason.USER_CLICKED_REVOKE_SYNC_CONSENT_SETTINGS, mOnSignOut, false);

        onView(withText(R.string.turn_off_sync_title))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testLegacyDialog_hasSyncingAccount() {
        setUpMocks();
        mocker.mock(PasswordManagerUtilBridgeJni.TEST_HOOKS, mPasswordManagerUtilBridgeNativeMock);
        mocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsNatives);
        when(mUserPrefsNatives.get(mProfile)).thenReturn(mPrefService);
        when(mPrefService.getBoolean(Pref.ALLOW_DELETING_BROWSER_HISTORY)).thenReturn(true);
        doReturn(true).when(mIdentityManagerMock).hasPrimaryAccount(ConsentLevel.SYNC);

        startSignOutFlow(SignoutReason.USER_CLICKED_SIGNOUT_SETTINGS, mOnSignOut, false);

        onView(withText(R.string.turn_off_sync_and_signout_title))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testSnackbarShownAfterSignOut() {
        setUpMocks();
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
                            SigninManager.SignOutCallback signOutCallback = args.getArgument(1);
                            signOutCallback.signOutComplete();
                            return null;
                        })
                .when(mSigninManagerMock)
                .signOut(eq(signOutReason), any(SigninManager.SignOutCallback.class), eq(false));

        startSignOutFlow(signOutReason, mOnSignOut, false);

        ThreadUtils.runOnUiThreadBlocking(
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
        verify(mOnSignOut).run();
    }

    @Test
    @MediumTest
    public void testUnsavedDataDialog() {
        setUpMocks();
        mUnsyncedDataTypes.add(DataType.BOOKMARKS);

        startSignOutFlow(SignoutReason.USER_CLICKED_SIGNOUT_SETTINGS, mOnSignOut, false);

        onView(withText(R.string.sign_out_unsaved_data_title))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
        onView(withText(R.string.sign_out_unsaved_data_message))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
        onView(withText(R.string.sign_out_unsaved_data_primary_button))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
        onView(withText(R.string.cancel)).inRoot(isDialog()).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testUnsavedDataDialogPrimaryButtonClick() {
        setUpMocks();
        mUnsyncedDataTypes.add(DataType.BOOKMARKS);
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
                .signOut(eq(signOutReason), any(SigninManager.SignOutCallback.class), eq(false));
        startSignOutFlow(signOutReason, mOnSignOut, false);
        onView(withText(R.string.sign_out_unsaved_data_title))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));

        onView(withText(R.string.sign_out_unsaved_data_primary_button))
                .inRoot(isDialog())
                .perform(click());

        verify(mOnSignOut).run();
    }

    @Test
    @MediumTest
    public void testUnsavedDataDialogSecondaryButtonClick() {
        setUpMocks();
        mUnsyncedDataTypes.add(DataType.BOOKMARKS);
        @SignoutReason int signOutReason = SignoutReason.USER_CLICKED_SIGNOUT_SETTINGS;
        startSignOutFlow(signOutReason, mOnSignOut, false);
        onView(withText(R.string.sign_out_unsaved_data_title))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));

        onView(withText(R.string.cancel)).inRoot(isDialog()).perform(click());

        verify(mSigninManagerMock, never()).runAfterOperationInProgress(any(Runnable.class));
    }

    @Test
    @MediumTest
    public void testSignOutConfirmDialog() {
        setUpMocks();

        startSignOutFlow(SignoutReason.USER_CLICKED_SIGNOUT_SETTINGS, mOnSignOut, true);
        onView(withText(R.string.sign_out_title)).inRoot(isDialog()).check(matches(isDisplayed()));
        onView(withText(R.string.sign_out_message))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
        onView(withText(R.string.sign_out)).inRoot(isDialog()).check(matches(isDisplayed()));
        onView(withText(R.string.cancel)).inRoot(isDialog()).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testSignOutConfirmDialogPrimaryButtonClick() {
        setUpMocks();
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
                .signOut(eq(signOutReason), any(SigninManager.SignOutCallback.class), eq(false));
        startSignOutFlow(signOutReason, mOnSignOut, true);
        onView(withText(R.string.sign_out_title)).inRoot(isDialog()).check(matches(isDisplayed()));

        onView(withText(R.string.sign_out)).inRoot(isDialog()).perform(click());

        ThreadUtils.runOnUiThreadBlocking(
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
        verify(mOnSignOut).run();
    }

    @Test
    @MediumTest
    public void testSignOutConfirmDialogSecondaryButtonClick() {
        setUpMocks();
        @SignoutReason int signOutReason = SignoutReason.USER_CLICKED_SIGNOUT_SETTINGS;
        startSignOutFlow(signOutReason, mOnSignOut, true);
        onView(withText(R.string.sign_out_title)).inRoot(isDialog()).check(matches(isDisplayed()));

        onView(withText(R.string.cancel)).inRoot(isDialog()).perform(click());

        verify(mSigninManagerMock, never()).runAfterOperationInProgress(any(Runnable.class));
    }

    @Test
    @MediumTest
    public void testSignOutConfirmDialogNowShownIfHasUnsavedData() {
        setUpMocks();
        mUnsyncedDataTypes.add(DataType.BOOKMARKS);

        startSignOutFlow(SignoutReason.USER_CLICKED_SIGNOUT_SETTINGS, mOnSignOut, true);

        onView(withText(R.string.sign_out_unsaved_data_title))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
    }

    private void setUpMocks() {
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProviderMock);
        doReturn(mIdentityManagerMock)
                .when(mIdentityServicesProviderMock)
                .getIdentityManager(mProfile);
        doReturn(true).when(mIdentityManagerMock).hasPrimaryAccount(ConsentLevel.SIGNIN);
        doReturn(mSigninManagerMock).when(mIdentityServicesProviderMock).getSigninManager(mProfile);
        SyncServiceFactory.setInstanceForTesting(mSyncService);
        doAnswer(
                        args -> {
                            args.getArgument(0, Callback.class).onResult(mUnsyncedDataTypes);
                            return null;
                        })
                .when(mSyncService)
                .getTypesWithUnsyncedData(any(Callback.class));

        ThreadUtils.runOnUiThreadBlocking(
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

    private void startSignOutFlow(
            @SignoutReason int signoutReason, Runnable onSignOut, boolean showConfirmDialog) {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        SignOutCoordinator.startSignOutFlow(
                                mActivityTestRule.getActivity(),
                                mProfile,
                                mFragmentManager,
                                mActivityTestRule.getActivity().getModalDialogManager(),
                                mSnackbarManager,
                                signoutReason,
                                showConfirmDialog,
                                onSignOut));
    }
}
