// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.pressBack;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.ViewMatchers.isRoot;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.doAnswer;
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

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridge;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridgeJni;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;

/**
 * This class regroups the integration tests for {@link ConfirmSyncDataStateMachine}.
 *
 * <p>In this class we use a real {@link ConfirmSyncDataStateMachineDelegate} to walk through
 * different states of the state machine by clicking on the dialogs shown with the delegate. This
 * way we tested the invocation of delegate inside state machine and vice versa.
 *
 * <p>In contrast, {@link ConfirmSyncDataStateMachineTest} takes a delegate mock to check the
 * interaction between the state machine and its delegate in one level.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class ConfirmSyncDataIntegrationTest extends BlankUiTestActivityTestCase {
    private static final String OLD_ACCOUNT_NAME = "test.account.old@gmail.com";
    private static final String NEW_ACCOUNT_NAME = "test.account.new@gmail.com";
    private static final String MANAGED_DOMAIN = "managed-domain.com";

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.LENIENT);

    @Rule public final JniMocker mJniMocker = new JniMocker();

    @Mock private SigninManager mSigninManagerMock;

    @Mock private IdentityServicesProvider mIdentityServicesProviderMock;

    @Mock private ConfirmSyncDataStateMachine.Listener mListenerMock;

    @Mock private PasswordManagerUtilBridge.Natives mPasswordManagerUtilBridgeNativeMock;

    @Mock private UserPrefs.Natives mUserPrefsNativeMock;

    @Mock private PrefService mPrefService;

    @Mock private Profile mProfile;

    private ConfirmSyncDataStateMachineDelegate mDelegate;

    @Before
    public void setUp() {
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProviderMock);
        mJniMocker.mock(
                PasswordManagerUtilBridgeJni.TEST_HOOKS, mPasswordManagerUtilBridgeNativeMock);
        mJniMocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsNativeMock);
        when(mUserPrefsNativeMock.get(mProfile)).thenReturn(mPrefService);
        when(IdentityServicesProvider.get().getSigninManager(any())).thenReturn(mSigninManagerMock);
        mDelegate =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return new ConfirmSyncDataStateMachineDelegate(
                                    getActivity(),
                                    mProfile,
                                    new ModalDialogManager(
                                            new AppModalPresenter(getActivity()),
                                            ModalDialogType.APP));
                        });
    }

    @Test
    @MediumTest
    public void testTwoDifferentNonManagedAccountsFlow() {
        mockSigninManagerIsAccountManaged(false);
        startConfirmSyncFlow(OLD_ACCOUNT_NAME, NEW_ACCOUNT_NAME);
        onView(withId(R.id.sync_keep_separate_choice)).inRoot(isDialog()).perform(click());
        onView(withText(R.string.continue_button)).perform(click());
        verify(mListenerMock).onConfirm(true, false);
        verify(mListenerMock, never()).onCancel();
    }

    @Test
    @MediumTest
    public void testTwoDifferentNonManagedAccountsCancelledFlow() {
        mockSigninManagerIsAccountManaged(false);
        startConfirmSyncFlow(OLD_ACCOUNT_NAME, NEW_ACCOUNT_NAME);
        onView(withId(R.id.sync_keep_separate_choice)).inRoot(isDialog()).perform(click());
        onView(isRoot()).perform(pressBack());
        verify(mListenerMock, never()).onConfirm(anyBoolean(), anyBoolean());
        verify(mListenerMock).onCancel();
    }

    @Test
    @MediumTest
    public void testNonManagedAccountToManagedAccountFlow() {
        mockSigninManagerIsAccountManaged(true);
        String managedNewAccountName = "test.account@" + MANAGED_DOMAIN;
        when(mSigninManagerMock.extractDomainName(managedNewAccountName))
                .thenReturn(MANAGED_DOMAIN);
        startConfirmSyncFlow(OLD_ACCOUNT_NAME, managedNewAccountName);
        onView(withId(R.id.sync_confirm_import_choice)).inRoot(isDialog()).perform(click());
        onView(withText(R.string.continue_button)).perform(click());
        onView(withText(R.string.continue_button)).inRoot(isDialog()).perform(click());
        verify(mListenerMock).onConfirm(false, true);
        verify(mListenerMock, never()).onCancel();
    }

    @Test
    @MediumTest
    public void testNonManagedAccountToManagedAccountCancelledFlow() {
        mockSigninManagerIsAccountManaged(true);
        String managedNewAccountName = "test.account@" + MANAGED_DOMAIN;
        when(mSigninManagerMock.extractDomainName(managedNewAccountName))
                .thenReturn(MANAGED_DOMAIN);
        startConfirmSyncFlow(OLD_ACCOUNT_NAME, managedNewAccountName);
        onView(withId(R.id.sync_keep_separate_choice)).inRoot(isDialog()).perform(click());
        onView(withText(R.string.continue_button)).perform(click());
        onView(isRoot()).perform(pressBack());
        verify(mListenerMock, never()).onConfirm(anyBoolean(), anyBoolean());
        verify(mListenerMock).onCancel();
    }

    @Test
    @MediumTest
    public void testTwoSameNonManagedAccountsFlow() {
        mockSigninManagerIsAccountManaged(false);
        startConfirmSyncFlow(OLD_ACCOUNT_NAME, OLD_ACCOUNT_NAME);
        onView(withId(R.id.sync_import_data_prompt)).check(doesNotExist());
        onView(withText(R.string.sign_in_managed_account)).check(doesNotExist());
        verify(mListenerMock).onConfirm(false, false);
        verify(mListenerMock, never()).onCancel();
    }

    @Test
    @MediumTest
    public void testNoPreviousAccountToManagedAccountFlow() {
        mockSigninManagerIsAccountManaged(true);
        String managedNewAccountName = "test.account@" + MANAGED_DOMAIN;
        when(mSigninManagerMock.extractDomainName(managedNewAccountName))
                .thenReturn(MANAGED_DOMAIN);
        startConfirmSyncFlow("", managedNewAccountName);
        onView(withText(R.string.continue_button)).inRoot(isDialog()).perform(click());
        verify(mListenerMock).onConfirm(false, true);
        verify(mListenerMock, never()).onCancel();
    }

    private void startConfirmSyncFlow(String oldAccountName, String newAccountName) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ConfirmSyncDataStateMachine stateMachine =
                            new ConfirmSyncDataStateMachine(
                                    mProfile,
                                    mDelegate,
                                    oldAccountName,
                                    newAccountName,
                                    mListenerMock);
                });
    }

    private void mockSigninManagerIsAccountManaged(boolean isAccountManaged) {
        doAnswer(
                        invocation -> {
                            Callback<Boolean> callback = invocation.getArgument(1);
                            callback.onResult(isAccountManaged);
                            return null;
                        })
                .when(mSigninManagerMock)
                .isAccountManaged(anyString(), any());
    }
}
