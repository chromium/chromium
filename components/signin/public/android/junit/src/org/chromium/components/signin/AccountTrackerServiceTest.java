// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.notNull;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.verify;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Spy;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.task.test.CustomShadowAsyncTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;

/**
 * Robolectric tests for {@link AccountTrackerService}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {CustomShadowAsyncTask.class})
public class AccountTrackerServiceTest {
    private static final long ACCOUNT_TRACKER_SERVICE_NATIVE = 10001L;
    private static final String ACCOUNT_EMAIL = "test@gmail.com";

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final JniMocker mocker = new JniMocker();

    @Spy
    private final FakeAccountManagerFacade mFakeAccountManagerFacade =
            new FakeAccountManagerFacade(null);

    @Mock
    private AccountTrackerService.Natives mNativeMock;

    @Before
    public void setUp() {
        AccountManagerFacadeProvider.setInstanceForTests(mFakeAccountManagerFacade);
        mocker.mock(AccountTrackerServiceJni.TEST_HOOKS, mNativeMock);
        mFakeAccountManagerFacade.addAccount(AccountUtils.createAccountFromName(ACCOUNT_EMAIL));
    }

    @After
    public void tearDown() {
        AccountManagerFacadeProvider.resetInstanceForTests();
    }

    @Test
    public void testCheckAndSeedAccountsTheFirstTime() {
        doAnswer(invocation -> {
            Assert.assertArrayEquals(
                    new String[] {mFakeAccountManagerFacade.getAccountGaiaId(ACCOUNT_EMAIL)},
                    invocation.getArgument(1));
            Assert.assertArrayEquals(new String[] {ACCOUNT_EMAIL}, invocation.getArgument(2));
            return true;
        })
                .when(mNativeMock)
                .seedAccountsInfo(eq(ACCOUNT_TRACKER_SERVICE_NATIVE), any(), any());
        AccountTrackerService service = new AccountTrackerService(ACCOUNT_TRACKER_SERVICE_NATIVE);
        service.checkAndSeedSystemAccounts();
        verify(mFakeAccountManagerFacade).addObserver(notNull());
        verify(mNativeMock).seedAccountsInfo(eq(ACCOUNT_TRACKER_SERVICE_NATIVE), any(), any());
    }
}
