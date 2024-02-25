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
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;

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
    public void testChildAccountStatusWhenNoAccountsOnDevice() {
        AccountUtils.checkChildAccountStatus(mFakeFacade, Collections.emptyList(), mListenerMock);
        verify(mListenerMock).onStatusReady(/* is_child_account= */ false, null);
    }

    @Test
    public void testChildAccountStatusWhenFirstAccountIsChildAndSecondIsEdu() {
        // This is a supported configuration (where the second account might be an EDU account).
        AccountUtils.checkChildAccountStatus(mFakeFacade, List.of(CHILD, EDU), mListenerMock);
        verify(mListenerMock).onStatusReady(/* is_child_account= */ true, CHILD);
    }

    @Test
    public void testChildAccountStatusWhenFirstAccountIsEduAndSecondIsChild() {
        // This is an unsupported configuration (the Kids Module ensures that if a child account
        // is present then it must be the default one).  This test is here for completeness.
        AccountUtils.checkChildAccountStatus(mFakeFacade, List.of(EDU, CHILD), mListenerMock);
        verify(mListenerMock).onStatusReady(/* is_child_account= */ false, null);
    }

    @Test
    public void testChildAccountStatusWhenTwoAdultAccountsOnDevice() {
        AccountUtils.checkChildAccountStatus(mFakeFacade, List.of(ADULT_1, ADULT_2), mListenerMock);
        verify(mListenerMock).onStatusReady(/* is_child_account= */ false, null);
    }

    @Test
    public void testChildAccountStatusWhenOnlyOneAdultAccountOnDevice() {
        AccountUtils.checkChildAccountStatus(mFakeFacade, List.of(ADULT_1), mListenerMock);
        verify(mListenerMock).onStatusReady(/* is_child_account= */ false, null);
    }

    @Test
    public void testChildAccountStatusWhenOnlyOneChildAccountOnDevice() {
        AccountUtils.checkChildAccountStatus(mFakeFacade, List.of(CHILD), mListenerMock);
        verify(mListenerMock).onStatusReady(/* is_child_account= */ true, CHILD);
    }
}
