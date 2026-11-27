// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.sync;

import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;
import org.chromium.components.signin.test.util.TestAccounts;

/**
 * Robolectric unit tests for {@link SyncServiceImpl}. Covers which accounts the account-keyed sync
 * settings are kept for, as the set of accounts on the device changes.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class SyncServiceImplTest {
    private static final long NATIVE_SYNC_SERVICE_ANDROID_BRIDGE = 1234L;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    private final FakeAccountManagerFacade mFakeAccountManagerFacade =
            new FakeAccountManagerFacade();

    @Mock private SyncServiceImpl.Natives mSyncServiceImplJniMock;

    @Before
    public void setUp() {
        AccountManagerFacadeProvider.setInstanceForTests(mFakeAccountManagerFacade);
        SyncServiceImplJni.setInstanceForTesting(mSyncServiceImplJniMock);
    }

    @Test
    public void testAccountsAvailableOnConstruction_keepsSettingsOnlyForThem() {
        mFakeAccountManagerFacade.addAccount(TestAccounts.ACCOUNT1);
        mFakeAccountManagerFacade.addAccount(TestAccounts.ACCOUNT2);

        createSyncService();

        verify(mSyncServiceImplJniMock)
                .keepAccountSettingsPrefsOnlyForUsers(
                        NATIVE_SYNC_SERVICE_ANDROID_BRIDGE,
                        new String[] {
                            TestAccounts.ACCOUNT1.getGaiaId().toString(),
                            TestAccounts.ACCOUNT2.getGaiaId().toString()
                        });
        verifyNoMoreInteractions(mSyncServiceImplJniMock);
    }

    @Test
    public void testAccountsNotLoadedOnConstruction_waitsForAccounts() {
        mFakeAccountManagerFacade.addAccount(TestAccounts.ACCOUNT1);
        try (var ignored =
                mFakeAccountManagerFacade.blockGetAccounts(/* postUnblockCallback= */ null)) {
            createSyncService();
            verifyNoMoreInteractions(mSyncServiceImplJniMock);
        }
        verify(mSyncServiceImplJniMock)
                .keepAccountSettingsPrefsOnlyForUsers(
                        NATIVE_SYNC_SERVICE_ANDROID_BRIDGE,
                        new String[] {TestAccounts.ACCOUNT1.getGaiaId().toString()});
        verifyNoMoreInteractions(mSyncServiceImplJniMock);
    }

    @Test
    public void testAccountsChanged_updatesAccountsToKeepSettingsFor() {
        createSyncService();
        verify(mSyncServiceImplJniMock)
                .keepAccountSettingsPrefsOnlyForUsers(
                        NATIVE_SYNC_SERVICE_ANDROID_BRIDGE, new String[0]);
        verifyNoMoreInteractions(mSyncServiceImplJniMock);
        clearInvocations(mSyncServiceImplJniMock);

        mFakeAccountManagerFacade.addAccount(TestAccounts.ACCOUNT1);
        verify(mSyncServiceImplJniMock)
                .keepAccountSettingsPrefsOnlyForUsers(
                        NATIVE_SYNC_SERVICE_ANDROID_BRIDGE,
                        new String[] {TestAccounts.ACCOUNT1.getGaiaId().toString()});
        verifyNoMoreInteractions(mSyncServiceImplJniMock);
        clearInvocations(mSyncServiceImplJniMock);

        mFakeAccountManagerFacade.addAccount(TestAccounts.ACCOUNT2);
        verify(mSyncServiceImplJniMock)
                .keepAccountSettingsPrefsOnlyForUsers(
                        NATIVE_SYNC_SERVICE_ANDROID_BRIDGE,
                        new String[] {
                            TestAccounts.ACCOUNT1.getGaiaId().toString(),
                            TestAccounts.ACCOUNT2.getGaiaId().toString()
                        });
        verifyNoMoreInteractions(mSyncServiceImplJniMock);
        clearInvocations(mSyncServiceImplJniMock);

        mFakeAccountManagerFacade.removeAccount(TestAccounts.ACCOUNT1.getId());
        verify(mSyncServiceImplJniMock)
                .keepAccountSettingsPrefsOnlyForUsers(
                        NATIVE_SYNC_SERVICE_ANDROID_BRIDGE,
                        new String[] {TestAccounts.ACCOUNT2.getGaiaId().toString()});
        verifyNoMoreInteractions(mSyncServiceImplJniMock);
        clearInvocations(mSyncServiceImplJniMock);

        mFakeAccountManagerFacade.removeAccount(TestAccounts.ACCOUNT2.getId());
        verify(mSyncServiceImplJniMock)
                .keepAccountSettingsPrefsOnlyForUsers(
                        NATIVE_SYNC_SERVICE_ANDROID_BRIDGE, new String[0]);
        verifyNoMoreInteractions(mSyncServiceImplJniMock);
    }

    @Test
    public void testDestroy_stopsObservingAccounts() {
        mFakeAccountManagerFacade.addAccount(TestAccounts.ACCOUNT1);
        SyncServiceImpl syncService = createSyncService();
        verify(mSyncServiceImplJniMock)
                .keepAccountSettingsPrefsOnlyForUsers(
                        NATIVE_SYNC_SERVICE_ANDROID_BRIDGE,
                        new String[] {TestAccounts.ACCOUNT1.getGaiaId().toString()});
        verifyNoMoreInteractions(mSyncServiceImplJniMock);
        clearInvocations(mSyncServiceImplJniMock);

        syncService.destroy();

        mFakeAccountManagerFacade.addAccount(TestAccounts.ACCOUNT2);
        mFakeAccountManagerFacade.removeAccount(TestAccounts.ACCOUNT1.getId());
        verifyNoMoreInteractions(mSyncServiceImplJniMock);
    }

    @Test
    public void testDestroyWhileAccountsNotLoaded_stopsObservingAccounts() {
        mFakeAccountManagerFacade.addAccount(TestAccounts.ACCOUNT1);
        try (var ignored =
                mFakeAccountManagerFacade.blockGetAccounts(/* postUnblockCallback= */ null)) {
            SyncServiceImpl syncService = createSyncService();
            syncService.destroy();
        }
        verifyNoMoreInteractions(mSyncServiceImplJniMock);
    }

    private SyncServiceImpl createSyncService() {
        return new SyncServiceImpl(NATIVE_SYNC_SERVICE_ANDROID_BRIDGE);
    }
}
