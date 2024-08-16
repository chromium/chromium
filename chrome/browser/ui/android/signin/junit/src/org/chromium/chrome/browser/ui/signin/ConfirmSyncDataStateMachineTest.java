// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;

/** Tests for {@link ConfirmSyncDataStateMachine}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ConfirmSyncDataStateMachineTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private ConfirmSyncDataStateMachineDelegate mDelegateMock;

    @Mock private ConfirmSyncDataStateMachine.Listener mStateMachineListenerMock;

    @Mock private SigninManager mSigninManagerMock;

    @Mock private Profile mProfile;

    @Captor private ArgumentCaptor<Callback<Boolean>> mCallbackArgument;

    @Captor
    private ArgumentCaptor<ConfirmManagedSyncDataDialogCoordinator.Listener> mListenerArgument;

    private final String mOldAccountName = "old.account.test@testdomain.com";

    private final String mNewAccountName = "new.account.test@testdomain.com";

    @Before
    public void setUp() {
        IdentityServicesProvider.setInstanceForTests(mock(IdentityServicesProvider.class));
        when(IdentityServicesProvider.get().getSigninManager(any())).thenReturn(mSigninManagerMock);
    }

    @Test(expected = AssertionError.class)
    public void testNewAccountNameCannotBeEmpty() {
        mMockitoRule.strictness(Strictness.LENIENT);
        ConfirmSyncDataStateMachine stateMachine =
                new ConfirmSyncDataStateMachine(
                        mProfile, mDelegateMock, mOldAccountName, null, mStateMachineListenerMock);
    }

    @Test
    public void testImportSyncDataDialogShownWhenOldAndNewAccountNamesAreDifferent() {
        ConfirmSyncDataStateMachine stateMachine =
                new ConfirmSyncDataStateMachine(
                        mProfile,
                        mDelegateMock,
                        mOldAccountName,
                        mNewAccountName,
                        mStateMachineListenerMock);
        verify(mDelegateMock)
                .showConfirmImportSyncDataDialog(
                        any(ConfirmImportSyncDataDialogCoordinator.Listener.class),
                        eq(mOldAccountName),
                        eq(mNewAccountName));
    }

    @Test
    public void testProgressDialogShownWhenOldAndNewAccountNamesAreEqual() {
        String oldAndNewAccountName = "test.old.new@testdomain.com";
        ConfirmSyncDataStateMachine stateMachine =
                new ConfirmSyncDataStateMachine(
                        mProfile,
                        mDelegateMock,
                        oldAndNewAccountName,
                        oldAndNewAccountName,
                        mStateMachineListenerMock);
        verify(mDelegateMock, never())
                .showConfirmImportSyncDataDialog(
                        any(ConfirmImportSyncDataDialogCoordinator.Listener.class),
                        anyString(),
                        anyString());
        verify(mDelegateMock)
                .showFetchManagementPolicyProgressDialog(
                        any(ConfirmSyncDataStateMachineDelegate.ProgressDialogListener.class));
    }

    @Test
    public void testProgressDialogShownWhenOldAccountNameIsEmpty() {
        ConfirmSyncDataStateMachine stateMachine =
                new ConfirmSyncDataStateMachine(
                        mProfile, mDelegateMock, null, mNewAccountName, mStateMachineListenerMock);
        verify(mDelegateMock, never())
                .showConfirmImportSyncDataDialog(
                        any(ConfirmImportSyncDataDialogCoordinator.Listener.class),
                        anyString(),
                        anyString());
        verify(mDelegateMock)
                .showFetchManagementPolicyProgressDialog(
                        any(ConfirmSyncDataStateMachineDelegate.ProgressDialogListener.class));
    }

    @Test
    public void testListenerConfirmedWhenNewAccountIsNotManaged() {
        mockSigninManagerIsAccountManaged(false);
        ConfirmSyncDataStateMachine stateMachine =
                new ConfirmSyncDataStateMachine(
                        mProfile, mDelegateMock, null, mNewAccountName, mStateMachineListenerMock);
        verify(mDelegateMock).dismissAllDialogs();
        verify(mStateMachineListenerMock).onConfirm(false, false);
    }

    @Test
    public void testManagedAccountDialogShownWhenNewAccountIsManaged() {
        mockSigninManagerIsAccountManaged(true);
        when(mSigninManagerMock.extractDomainName(anyString())).thenReturn(mNewAccountName);
        ConfirmSyncDataStateMachine stateMachine =
                new ConfirmSyncDataStateMachine(
                        mProfile, mDelegateMock, null, mNewAccountName, mStateMachineListenerMock);
        verify(mDelegateMock)
                .showSignInToManagedAccountDialog(mListenerArgument.capture(), eq(mNewAccountName));
        mListenerArgument.getValue().onConfirm();
        verify(mStateMachineListenerMock).onConfirm(false, true);
    }

    @Test
    public void testWhenManagedAccountStatusIsFetchedAfterNewAccountDialog() {
        String newAccountName = "test.account@manageddomain.com";
        String domain = "manageddomain.com";
        when(mSigninManagerMock.extractDomainName(newAccountName)).thenReturn(domain);
        ConfirmSyncDataStateMachine stateMachine =
                new ConfirmSyncDataStateMachine(
                        mProfile, mDelegateMock, null, newAccountName, mStateMachineListenerMock);
        verify(mDelegateMock, never())
                .showSignInToManagedAccountDialog(
                        any(ConfirmManagedSyncDataDialogCoordinator.Listener.class), anyString());
        verify(mSigninManagerMock)
                .isAccountManaged(eq(newAccountName), mCallbackArgument.capture());
        Callback<Boolean> callback = mCallbackArgument.getValue();
        callback.onResult(true);
        verify(mDelegateMock)
                .showSignInToManagedAccountDialog(mListenerArgument.capture(), eq(domain));
        mListenerArgument.getValue().onConfirm();
        verify(mStateMachineListenerMock).onConfirm(false, true);
    }

    @Test
    public void testCancelWhenIsNotBeingDestroyed() {
        ConfirmSyncDataStateMachine stateMachine =
                new ConfirmSyncDataStateMachine(
                        mProfile,
                        mDelegateMock,
                        mOldAccountName,
                        mNewAccountName,
                        mStateMachineListenerMock);
        stateMachine.onCancel();
        verify(mStateMachineListenerMock).onCancel();
        verify(mDelegateMock).dismissAllDialogs();
    }

    @Test
    public void testCancelWhenIsBeingDestroyed() {
        ConfirmSyncDataStateMachine stateMachine =
                new ConfirmSyncDataStateMachine(
                        mProfile,
                        mDelegateMock,
                        mOldAccountName,
                        mNewAccountName,
                        mStateMachineListenerMock);
        stateMachine.cancel(true);
        verify(mStateMachineListenerMock, never()).onCancel();
        verify(mDelegateMock, never()).dismissAllDialogs();
    }

    @Test(expected = IllegalStateException.class)
    public void testStateCannotChangeOnceDone() {
        ConfirmSyncDataStateMachine stateMachine =
                new ConfirmSyncDataStateMachine(
                        mProfile,
                        mDelegateMock,
                        mOldAccountName,
                        mNewAccountName,
                        mStateMachineListenerMock);
        stateMachine.cancel(true);
        stateMachine.onConfirm();
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
