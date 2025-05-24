// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin;

import static org.mockito.Mockito.verify;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.signin.AccountManagerFacade.ChildAccountStatusListener;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;
import org.chromium.components.signin.test.util.TestAccounts;

import java.util.Collections;
import java.util.List;

/** Unit tests for {@link AccountUtils} */
@RunWith(BaseRobolectricTestRunner.class)
public class AccountUtilsTest {
    private final FakeAccountManagerFacade mFakeFacade = new FakeAccountManagerFacade();

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ChildAccountStatusListener mListenerMock;

    @Test
    public void testIsSubjectToParentalControlsWhenNoAccountsOnDevice() {
        AccountUtils.checkIsSubjectToParentalControls(
                mFakeFacade, Collections.emptyList(), mListenerMock);
        verify(mListenerMock).onStatusReady(/* isChild= */ false, null);
    }

    @Test
    public void testIsSubjectToParentalControlsWhenFirstAccountIsChildAndSecondIsEdu() {
        mFakeFacade.addAccount(TestAccounts.CHILD_ACCOUNT);
        mFakeFacade.addAccount(TestAccounts.ACCOUNT1);
        // This is a supported configuration (where the second account might be an EDU account).
        AccountUtils.checkIsSubjectToParentalControls(
                mFakeFacade,
                List.of(TestAccounts.CHILD_ACCOUNT, TestAccounts.ACCOUNT1),
                mListenerMock);
        verify(mListenerMock).onStatusReady(/* isChild= */ true, TestAccounts.CHILD_ACCOUNT);
    }

    @Test
    public void testIsSubjectToParentalControlsWhenFirstAccountIsEduAndSecondIsChild() {
        mFakeFacade.addAccount(TestAccounts.ACCOUNT1);
        mFakeFacade.addAccount(TestAccounts.CHILD_ACCOUNT);
        // This is an unsupported configuration (the Kids Module ensures that if a child account
        // is present then it must be the default one).  This test is here for completeness.
        AccountUtils.checkIsSubjectToParentalControls(
                mFakeFacade,
                List.of(TestAccounts.ACCOUNT1, TestAccounts.CHILD_ACCOUNT),
                mListenerMock);
        verify(mListenerMock).onStatusReady(/* isChild= */ false, null);
    }

    @Test
    public void testIsSubjectToParentalControlsWhenTwoAdultAccountsOnDevice() {
        mFakeFacade.addAccount(TestAccounts.ACCOUNT1);
        mFakeFacade.addAccount(TestAccounts.ACCOUNT2);

        AccountUtils.checkIsSubjectToParentalControls(
                mFakeFacade, List.of(TestAccounts.ACCOUNT1, TestAccounts.ACCOUNT2), mListenerMock);
        verify(mListenerMock).onStatusReady(/* isChild= */ false, null);
    }

    @Test
    public void testIsSubjectToParentalControlsWhenOnlyOneAdultAccountOnDevice() {
        mFakeFacade.addAccount(TestAccounts.ACCOUNT1);
        AccountUtils.checkIsSubjectToParentalControls(
                mFakeFacade, List.of(TestAccounts.ACCOUNT1), mListenerMock);
        verify(mListenerMock).onStatusReady(/* isChild= */ false, null);
    }

    @Test
    public void testIsSubjectToParentalControlsWhenOnlyOneChildAccountOnDevice() {
        mFakeFacade.addAccount(TestAccounts.CHILD_ACCOUNT);
        AccountUtils.checkIsSubjectToParentalControls(
                mFakeFacade, List.of(TestAccounts.CHILD_ACCOUNT), mListenerMock);
        verify(mListenerMock).onStatusReady(/* isChild= */ true, TestAccounts.CHILD_ACCOUNT);
    }
}
