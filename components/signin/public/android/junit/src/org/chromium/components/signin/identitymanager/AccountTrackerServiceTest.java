// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.identitymanager;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.accounts.Account;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.Spy;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.Promise;
import org.chromium.base.task.test.CustomShadowAsyncTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;

import java.util.List;

/**
 * Robolectric tests for {@link AccountTrackerService}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {CustomShadowAsyncTask.class})
@LooperMode(LooperMode.Mode.LEGACY)
public class AccountTrackerServiceTest {
    private static final long ACCOUNT_TRACKER_SERVICE_NATIVE = 10001L;
    private static final String ACCOUNT_EMAIL = "test@gmail.com";

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Rule
    public final JniMocker mocker = new JniMocker();

    // TODO(https://crbug.com/1336704): Use mock instead of spy.
    @Spy
    private final FakeAccountManagerFacade mFakeAccountManagerFacade =
            new FakeAccountManagerFacade();

    @Mock
    private AccountTrackerService.Natives mNativeMock;

    @Mock
    private Runnable mRunnableMock;

    @Mock
    private AccountTrackerService.Observer mObserverMock;

    @Captor
    private ArgumentCaptor<CoreAccountInfo[]> mCoreAccountInfosArrayCaptor;

    @Captor
    private ArgumentCaptor<List<CoreAccountInfo>> mCoreAccountInfosListCaptor;

    private AccountTrackerService mService;
    private CoreAccountInfo mCoreAccountInfo;

    @Before
    public void setUp() {
        AccountManagerFacadeProvider.setInstanceForTests(mFakeAccountManagerFacade);
        mocker.mock(AccountTrackerServiceJni.TEST_HOOKS, mNativeMock);
        mFakeAccountManagerFacade.addAccount(AccountUtils.createAccountFromName(ACCOUNT_EMAIL));
        mService = new AccountTrackerService(ACCOUNT_TRACKER_SERVICE_NATIVE);
        mCoreAccountInfo = CoreAccountInfo.createFromEmailAndGaiaId(
                ACCOUNT_EMAIL, mFakeAccountManagerFacade.getAccountGaiaId(ACCOUNT_EMAIL));
    }

    @After
    public void tearDown() {
        AccountManagerFacadeProvider.resetInstanceForTests();
    }

    @Test
    public void testAccountManagerFacadeObserverAddedOnCreate() {
        AccountManagerFacade accountManagerFacadeMock = Mockito.mock(AccountManagerFacade.class);
        AccountManagerFacadeProvider.setInstanceForTests(accountManagerFacadeMock);

        mService = new AccountTrackerService(ACCOUNT_TRACKER_SERVICE_NATIVE);

        verify(accountManagerFacadeMock).addObserver(mService);
    }

    @Test
    public void testAccountManagerFacadeObserverRemovedOnDestroy() {
        AccountManagerFacade accountManagerFacadeMock = Mockito.mock(AccountManagerFacade.class);
        AccountManagerFacadeProvider.setInstanceForTests(accountManagerFacadeMock);
        mService = new AccountTrackerService(ACCOUNT_TRACKER_SERVICE_NATIVE);

        mService.destroy();

        verify(accountManagerFacadeMock).removeObserver(mService);
    }

    @Test
    public void testInvalidatingAccountSeedingStatusReseedsAccounts() {
        mService.invalidateAccountsSeedingStatus();

        verify(mNativeMock).seedAccountsInfo(anyLong(), any());
    }

    @Test
    public void testSeedAccountsIfNeededBeforeAccountsAreSeeded() {
        mService.seedAccountsIfNeeded(mRunnableMock);

        verify(mNativeMock)
                .seedAccountsInfo(
                        eq(ACCOUNT_TRACKER_SERVICE_NATIVE), mCoreAccountInfosArrayCaptor.capture());
        Assert.assertArrayEquals(
                new CoreAccountInfo[] {mCoreAccountInfo}, mCoreAccountInfosArrayCaptor.getValue());
        verify(mRunnableMock).run();
    }

    @Test
    public void testSeedAccountsIfNeededWhenSeedingIsInProgress() {
        Promise<List<CoreAccountInfo>> coreAccountInfoPromise = new Promise();
        doReturn(coreAccountInfoPromise).when(mFakeAccountManagerFacade).getCoreAccountInfos();

        mService.seedAccountsIfNeeded(() -> {});
        // Call again while seeding is in progress.
        mService.seedAccountsIfNeeded(mRunnableMock);
        verify(mNativeMock, never()).seedAccountsInfo(anyLong(), any());
        coreAccountInfoPromise.fulfill(List.of(mCoreAccountInfo));

        verify(mNativeMock)
                .seedAccountsInfo(
                        eq(ACCOUNT_TRACKER_SERVICE_NATIVE), mCoreAccountInfosArrayCaptor.capture());
        Assert.assertArrayEquals(
                new CoreAccountInfo[] {mCoreAccountInfo}, mCoreAccountInfosArrayCaptor.getValue());
        verify(mRunnableMock).run();
    }

    @Test
    public void testSeedAccountsIfNeededAfterAccountsAreSeeded() {
        mService.seedAccountsIfNeeded(() -> {});

        mService.seedAccountsIfNeeded(mRunnableMock);

        // Accounts should be seeded only once
        verify(mNativeMock)
                .seedAccountsInfo(
                        eq(ACCOUNT_TRACKER_SERVICE_NATIVE), mCoreAccountInfosArrayCaptor.capture());
        Assert.assertArrayEquals(
                new CoreAccountInfo[] {mCoreAccountInfo}, mCoreAccountInfosArrayCaptor.getValue());
        verify(mRunnableMock).run();
    }

    @Test
    public void testAddingNewAccountTriggersSeedingAccounts() {
        mService.seedAccountsIfNeeded(() -> {});
        mService.addObserver(mObserverMock);
        final Account newAccount = AccountUtils.createAccountFromName("test2@gmail.com");
        verify(mNativeMock).seedAccountsInfo(eq(ACCOUNT_TRACKER_SERVICE_NATIVE), any());

        mFakeAccountManagerFacade.addAccount(newAccount);

        verify(mNativeMock, times(2)).seedAccountsInfo(eq(ACCOUNT_TRACKER_SERVICE_NATIVE), any());
        // Verify the observer is invoked with correct arguments
        verify(mObserverMock).onAccountsSeeded(mCoreAccountInfosListCaptor.capture(), eq(true));
        final CoreAccountInfo coreAccountInfo = CoreAccountInfo.createFromEmailAndGaiaId(
                ACCOUNT_EMAIL, mFakeAccountManagerFacade.getAccountGaiaId(ACCOUNT_EMAIL));
        final CoreAccountInfo newCoreAccountInfo = CoreAccountInfo.createFromEmailAndGaiaId(
                newAccount.name, mFakeAccountManagerFacade.getAccountGaiaId(newAccount.name));
        Assert.assertArrayEquals(new CoreAccountInfo[] {coreAccountInfo, newCoreAccountInfo},
                mCoreAccountInfosListCaptor.getValue().toArray(new CoreAccountInfo[0]));
    }

    @Test
    public void testSeedAccountsWithObserverAttached() {
        mService.addObserver(mObserverMock);
        verify(mObserverMock, never()).onAccountsSeeded(any(), anyBoolean());

        mService.seedAccountsIfNeeded(() -> {});

        verify(mObserverMock).onAccountsSeeded(mCoreAccountInfosListCaptor.capture(), eq(false));
        final CoreAccountInfo account = CoreAccountInfo.createFromEmailAndGaiaId(
                ACCOUNT_EMAIL, mFakeAccountManagerFacade.getAccountGaiaId(ACCOUNT_EMAIL));
        Assert.assertArrayEquals(new CoreAccountInfo[] {account},
                mCoreAccountInfosListCaptor.getValue().toArray(new CoreAccountInfo[0]));
    }

    @Test
    public void testSeedAccountsWithObserverRemoved() {
        mService.addObserver(mObserverMock);
        mService.removeObserver(mObserverMock);

        mService.seedAccountsIfNeeded(() -> {});

        verify(mObserverMock, never()).onAccountsSeeded(any(), anyBoolean());
    }
}
