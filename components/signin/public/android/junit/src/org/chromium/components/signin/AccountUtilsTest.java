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
import org.chromium.base.test.util.Features;
import org.chromium.components.signin.AccountManagerFacade.ChildAccountStatusListener;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;
import org.chromium.components.signin.test.util.TestAccounts;

import java.util.Collections;
import java.util.List;

/** Unit tests for {@link AccountUtils} */
@RunWith(BaseRobolectricTestRunner.class)
public class AccountUtilsTest {
    private static final CoreAccountInfo CHILD =
            CoreAccountInfo.createFromEmailAndGaiaId(
                    FakeAccountManagerFacade.generateChildEmail("account@gmail.com"),
                    "notUsedGaiaId");
    private static final CoreAccountInfo ADULT_1 =
            CoreAccountInfo.createFromEmailAndGaiaId("adult.account1@gmail.com", "notUsedGaiaId");
    private static final CoreAccountInfo ADULT_2 =
            CoreAccountInfo.createFromEmailAndGaiaId("adult.account2@gmail.com", "notUsedGaiaId");
    private static final CoreAccountInfo EDU =
            CoreAccountInfo.createFromEmailAndGaiaId("edu.account@gmail.com", "notUsedGaiaId");

    private final FakeAccountManagerFacade mFakeFacade = new FakeAccountManagerFacade();

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ChildAccountStatusListener mListenerMock;

    @Test
    @Features.DisableFeatures(SigninFeatures.FORCE_SUPERVISED_SIGNIN_WITH_CAPABILITIES)
    public void testChildAccountStatusWhenNoAccountsOnDevice() {
        AccountUtils.checkChildAccountStatus(mFakeFacade, Collections.emptyList(), mListenerMock);
        verify(mListenerMock).onStatusReady(/* is_child_account= */ false, null);
    }

    @Test
    @Features.DisableFeatures(SigninFeatures.FORCE_SUPERVISED_SIGNIN_WITH_CAPABILITIES)
    public void testChildAccountStatusWhenFirstAccountIsChildAndSecondIsEdu() {
        // This is a supported configuration (where the second account might be an EDU account).
        AccountUtils.checkChildAccountStatus(mFakeFacade, List.of(CHILD, EDU), mListenerMock);
        verify(mListenerMock).onStatusReady(/* is_child_account= */ true, CHILD);
    }

    @Test
    @Features.DisableFeatures(SigninFeatures.FORCE_SUPERVISED_SIGNIN_WITH_CAPABILITIES)
    public void testChildAccountStatusWhenFirstAccountIsEduAndSecondIsChild() {
        // This is an unsupported configuration (the Kids Module ensures that if a child account
        // is present then it must be the default one).  This test is here for completeness.
        AccountUtils.checkChildAccountStatus(mFakeFacade, List.of(EDU, CHILD), mListenerMock);
        verify(mListenerMock).onStatusReady(/* is_child_account= */ false, null);
    }

    @Test
    @Features.DisableFeatures(SigninFeatures.FORCE_SUPERVISED_SIGNIN_WITH_CAPABILITIES)
    public void testChildAccountStatusWhenTwoAdultAccountsOnDevice() {
        AccountUtils.checkChildAccountStatus(mFakeFacade, List.of(ADULT_1, ADULT_2), mListenerMock);
        verify(mListenerMock).onStatusReady(/* is_child_account= */ false, null);
    }

    @Test
    @Features.DisableFeatures(SigninFeatures.FORCE_SUPERVISED_SIGNIN_WITH_CAPABILITIES)
    public void testChildAccountStatusWhenOnlyOneAdultAccountOnDevice() {
        AccountUtils.checkChildAccountStatus(mFakeFacade, List.of(ADULT_1), mListenerMock);
        verify(mListenerMock).onStatusReady(/* is_child_account= */ false, null);
    }

    @Test
    @Features.DisableFeatures(SigninFeatures.FORCE_SUPERVISED_SIGNIN_WITH_CAPABILITIES)
    public void testChildAccountStatusWhenOnlyOneChildAccountOnDevice() {
        AccountUtils.checkChildAccountStatus(mFakeFacade, List.of(CHILD), mListenerMock);
        verify(mListenerMock).onStatusReady(/* is_child_account= */ true, CHILD);
    }

    @Test
    @Features.EnableFeatures(SigninFeatures.FORCE_SUPERVISED_SIGNIN_WITH_CAPABILITIES)
    public void testIsSubjectToParentalControlsWhenNoAccountsOnDevice() {
        AccountUtils.checkIsSubjectToParentalControls(
                mFakeFacade, Collections.emptyList(), mListenerMock);
        verify(mListenerMock).onStatusReady(/* is_child_account= */ false, null);
    }

    @Test
    @Features.EnableFeatures(SigninFeatures.FORCE_SUPERVISED_SIGNIN_WITH_CAPABILITIES)
    public void testIsSubjectToParentalControlsWhenFirstAccountIsChildAndSecondIsEdu() {
        mFakeFacade.addAccount(TestAccounts.CHILD_ACCOUNT);
        mFakeFacade.addAccount(TestAccounts.ACCOUNT1);
        // This is a supported configuration (where the second account might be an EDU account).
        AccountUtils.checkIsSubjectToParentalControls(
                mFakeFacade,
                List.of(TestAccounts.CHILD_ACCOUNT, TestAccounts.ACCOUNT1),
                mListenerMock);
        verify(mListenerMock)
                .onStatusReady(/* is_child_account= */ true, TestAccounts.CHILD_ACCOUNT);
    }

    @Test
    @Features.EnableFeatures(SigninFeatures.FORCE_SUPERVISED_SIGNIN_WITH_CAPABILITIES)
    public void testIsSubjectToParentalControlsWhenFirstAccountIsEduAndSecondIsChild() {
        mFakeFacade.addAccount(TestAccounts.ACCOUNT1);
        mFakeFacade.addAccount(TestAccounts.CHILD_ACCOUNT);
        // This is an unsupported configuration (the Kids Module ensures that if a child account
        // is present then it must be the default one).  This test is here for completeness.
        AccountUtils.checkIsSubjectToParentalControls(
                mFakeFacade,
                List.of(TestAccounts.ACCOUNT1, TestAccounts.CHILD_ACCOUNT),
                mListenerMock);
        verify(mListenerMock).onStatusReady(/* is_child_account= */ false, null);
    }

    @Test
    @Features.EnableFeatures(SigninFeatures.FORCE_SUPERVISED_SIGNIN_WITH_CAPABILITIES)
    public void testIsSubjectToParentalControlsWhenTwoAdultAccountsOnDevice() {
        mFakeFacade.addAccount(TestAccounts.ACCOUNT1);
        mFakeFacade.addAccount(TestAccounts.ACCOUNT2);

        AccountUtils.checkIsSubjectToParentalControls(
                mFakeFacade, List.of(TestAccounts.ACCOUNT1, TestAccounts.ACCOUNT2), mListenerMock);
        verify(mListenerMock).onStatusReady(/* is_child_account= */ false, null);
    }

    @Test
    @Features.EnableFeatures(SigninFeatures.FORCE_SUPERVISED_SIGNIN_WITH_CAPABILITIES)
    public void testIsSubjectToParentalControlsWhenOnlyOneAdultAccountOnDevice() {
        mFakeFacade.addAccount(TestAccounts.ACCOUNT1);
        AccountUtils.checkIsSubjectToParentalControls(
                mFakeFacade, List.of(TestAccounts.ACCOUNT1), mListenerMock);
        verify(mListenerMock).onStatusReady(/* is_child_account= */ false, null);
    }

    @Test
    @Features.EnableFeatures(SigninFeatures.FORCE_SUPERVISED_SIGNIN_WITH_CAPABILITIES)
    public void testIsSubjectToParentalControlsWhenOnlyOneChildAccountOnDevice() {
        mFakeFacade.addAccount(TestAccounts.CHILD_ACCOUNT);
        AccountUtils.checkIsSubjectToParentalControls(
                mFakeFacade, List.of(TestAccounts.CHILD_ACCOUNT), mListenerMock);
        verify(mListenerMock)
                .onStatusReady(/* is_child_account= */ true, TestAccounts.CHILD_ACCOUNT);
    }
}
