// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin;

import static org.mockito.Mockito.verify;

import android.accounts.Account;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.signin.AccountManagerFacade.ChildAccountStatusListener;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;

import java.util.Collections;
import java.util.List;

/** Unit tests for {@link AccountUtils} */
@RunWith(BaseRobolectricTestRunner.class)
public class AccountUtilsTest {
    private static final Account CHILD_ACCOUNT1 = AccountUtils.createAccountFromName(
            FakeAccountManagerFacade.generateChildEmail("account1@gmail.com"));
    private static final Account CHILD_ACCOUNT2 = AccountUtils.createAccountFromName(
            FakeAccountManagerFacade.generateChildEmail("account2@gmail.com"));
    private static final Account ADULT_ACCOUNT1 =
            AccountUtils.createAccountFromName("adult.account1@gmail.com");
    private static final Account ADULT_ACCOUNT2 =
            AccountUtils.createAccountFromName("adult.account2@gmail.com");
    private static final Account EDU_ACCOUNT =
            AccountUtils.createAccountFromName("edu.account1@school.com");

    private final FakeAccountManagerFacade mFakeFacade = new FakeAccountManagerFacade();

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    private ChildAccountStatusListener mListenerMock;

    @Test
    public void testChildAccountStatusWhenNoAccountsOnDevice() {
        AccountUtils.checkChildAccountStatus(mFakeFacade, Collections.emptyList(), mListenerMock);
        verify(mListenerMock).onStatusReady(/*is_child_account=*/false, null);
    }

    @Test
    public void testChildAccountStatusWhenFirstAccountIsChildAndSecondIsEdu() {
        // This is a supported configuration (where the second account might be an EDU account).
        AccountUtils.checkChildAccountStatus(
                mFakeFacade, List.of(CHILD_ACCOUNT1, EDU_ACCOUNT), mListenerMock);
        verify(mListenerMock).onStatusReady(/*is_child_account=*/true, CHILD_ACCOUNT1);
    }

    @Test
    public void testChildAccountStatusWhenFirstAccountIsEduAndSecondIsChild() {
        // This is an unsupported configuration (the Kids Module ensures that if a child account
        // is present then it must be the default one).  This test is here for completeness.
        AccountUtils.checkChildAccountStatus(
                mFakeFacade, List.of(EDU_ACCOUNT, CHILD_ACCOUNT1), mListenerMock);
        verify(mListenerMock).onStatusReady(/*is_child_account=*/false, null);
    }

    @Test
    public void testChildAccountStatusWhenTwoAdultAccountsOnDevice() {
        AccountUtils.checkChildAccountStatus(
                mFakeFacade, List.of(ADULT_ACCOUNT1, ADULT_ACCOUNT2), mListenerMock);
        verify(mListenerMock).onStatusReady(/*is_child_account=*/false, null);
    }

    @Test
    public void testChildAccountStatusWhenOnlyOneAdultAccountOnDevice() {
        AccountUtils.checkChildAccountStatus(mFakeFacade, List.of(ADULT_ACCOUNT1), mListenerMock);
        verify(mListenerMock).onStatusReady(/*is_child_account=*/false, null);
    }

    @Test
    public void testChildAccountStatusWhenOnlyOneChildAccountOnDevice() {
        AccountUtils.checkChildAccountStatus(mFakeFacade, List.of(CHILD_ACCOUNT1), mListenerMock);
        verify(mListenerMock).onStatusReady(/*is_child_account=*/true, CHILD_ACCOUNT1);
    }
}
