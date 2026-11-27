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
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.identitymanager.IdentityManager.RefreshTokensLoadingState;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;
import org.chromium.components.signin.test.util.FakeIdentityManager;
import org.chromium.components.signin.test.util.TestAccounts;

/**
 * Robolectric unit tests for {@link SyncServiceImpl}. Covers which accounts the account-keyed sync
 * settings are kept for, as the set of accounts on the device changes.
 *
 * <p>TODO(crbug.com/493130564): Remove the "_legacy" tests after
 * MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS_PART2 launch.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class SyncServiceImplTest {
    private static final long NATIVE_SYNC_SERVICE_ANDROID_BRIDGE = 1234L;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    private final FakeAccountManagerFacade mFakeAccountManagerFacade =
            new FakeAccountManagerFacade();
    private final FakeIdentityManager mFakeIdentityManager = new FakeIdentityManager();

    @Mock private SyncServiceImpl.Natives mSyncServiceImplJniMock;

    @Before
    public void setUp() {
        AccountManagerFacadeProvider.setInstanceForTests(mFakeAccountManagerFacade);
        SyncServiceImplJni.setInstanceForTesting(mSyncServiceImplJniMock);
    }

    @Test
    @EnableFeatures(SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS_PART2)
    public void testAccountsAvailableOnConstruction_keepsSettingsOnlyForThem() {
        mFakeIdentityManager.addOrUpdateExtendedAccountInfo(TestAccounts.ACCOUNT1);
        mFakeIdentityManager.addOrUpdateExtendedAccountInfo(TestAccounts.ACCOUNT2);

        createSyncService();

        verify(mSyncServiceImplJniMock)
                .keepAccountSettingsPrefsOnlyForUsers(
                        NATIVE_SYNC_SERVICE_ANDROID_BRIDGE,
                        new String[] {
                            TestAccounts.ACCOUNT1.getGaiaId().toString(),
                            TestAccounts.ACCOUNT2.getGaiaId().toString()
                        });
    }

    @Test
    @DisableFeatures(SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS_PART2)
    public void testAccountsAvailableOnConstruction_keepsSettingsOnlyForThem_legacy() {
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
    @EnableFeatures(SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS_PART2)
    public void testAccountsNotLoadedOnConstruction_waitsForAccounts() {
        mFakeIdentityManager.addOrUpdateExtendedAccountInfo(TestAccounts.ACCOUNT1);
        mFakeIdentityManager.setRefreshTokensLoadingState(RefreshTokensLoadingState.IN_PROGRESS);

        createSyncService();
        verifyNoMoreInteractions(mSyncServiceImplJniMock);

        mFakeIdentityManager.setRefreshTokensLoadingState(RefreshTokensLoadingState.SUCCEEDED);
        verify(mSyncServiceImplJniMock)
                .keepAccountSettingsPrefsOnlyForUsers(
                        NATIVE_SYNC_SERVICE_ANDROID_BRIDGE,
                        new String[] {TestAccounts.ACCOUNT1.getGaiaId().toString()});
    }

    @Test
    @DisableFeatures(SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS_PART2)
    public void testAccountsNotLoadedOnConstruction_waitsForAccounts_legacy() {
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
    @EnableFeatures(SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS_PART2)
    public void testAccountsLoadingFailed_doesNotTouchSettings() {
        mFakeIdentityManager.addOrUpdateExtendedAccountInfo(TestAccounts.ACCOUNT1);
        mFakeIdentityManager.setRefreshTokensLoadingState(RefreshTokensLoadingState.IN_PROGRESS);

        createSyncService();
        mFakeIdentityManager.setRefreshTokensLoadingState(RefreshTokensLoadingState.FAILED);

        verifyNoMoreInteractions(mSyncServiceImplJniMock);
    }

    @Test
    @EnableFeatures(SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS_PART2)
    public void testAccountsChanged_updatesAccountsToKeepSettingsFor() {
        createSyncService();
        verify(mSyncServiceImplJniMock)
                .keepAccountSettingsPrefsOnlyForUsers(
                        NATIVE_SYNC_SERVICE_ANDROID_BRIDGE, new String[0]);
        verifyNoMoreInteractions(mSyncServiceImplJniMock);
        clearInvocations(mSyncServiceImplJniMock);

        mFakeIdentityManager.addOrUpdateExtendedAccountInfo(TestAccounts.ACCOUNT1);
        verify(mSyncServiceImplJniMock)
                .keepAccountSettingsPrefsOnlyForUsers(
                        NATIVE_SYNC_SERVICE_ANDROID_BRIDGE,
                        new String[] {TestAccounts.ACCOUNT1.getGaiaId().toString()});
        clearInvocations(mSyncServiceImplJniMock);

        mFakeIdentityManager.addOrUpdateExtendedAccountInfo(TestAccounts.ACCOUNT2);
        verify(mSyncServiceImplJniMock)
                .keepAccountSettingsPrefsOnlyForUsers(
                        NATIVE_SYNC_SERVICE_ANDROID_BRIDGE,
                        new String[] {
                            TestAccounts.ACCOUNT1.getGaiaId().toString(),
                            TestAccounts.ACCOUNT2.getGaiaId().toString()
                        });
        clearInvocations(mSyncServiceImplJniMock);

        mFakeIdentityManager.removeAccount(TestAccounts.ACCOUNT1.getId());
        verify(mSyncServiceImplJniMock)
                .keepAccountSettingsPrefsOnlyForUsers(
                        NATIVE_SYNC_SERVICE_ANDROID_BRIDGE,
                        new String[] {TestAccounts.ACCOUNT2.getGaiaId().toString()});
        clearInvocations(mSyncServiceImplJniMock);

        mFakeIdentityManager.removeAccount(TestAccounts.ACCOUNT2.getId());
        verify(mSyncServiceImplJniMock)
                .keepAccountSettingsPrefsOnlyForUsers(
                        NATIVE_SYNC_SERVICE_ANDROID_BRIDGE, new String[0]);
    }

    @Test
    @DisableFeatures(SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS_PART2)
    public void testAccountsChanged_updatesAccountsToKeepSettingsFor_legacy() {
        createSyncService();
        verify(mSyncServiceImplJniMock)
                .keepAccountSettingsPrefsOnlyForUsers(
                        NATIVE_SYNC_SERVICE_ANDROID_BRIDGE, new String[0]);
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
    @EnableFeatures(SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS_PART2)
    public void testDestroy_stopsObservingAccounts() {
        mFakeIdentityManager.addOrUpdateExtendedAccountInfo(TestAccounts.ACCOUNT1);
        SyncServiceImpl syncService = createSyncService();
        verify(mSyncServiceImplJniMock)
                .keepAccountSettingsPrefsOnlyForUsers(
                        NATIVE_SYNC_SERVICE_ANDROID_BRIDGE,
                        new String[] {TestAccounts.ACCOUNT1.getGaiaId().toString()});
        clearInvocations(mSyncServiceImplJniMock);

        syncService.destroy();

        mFakeIdentityManager.addOrUpdateExtendedAccountInfo(TestAccounts.ACCOUNT2);
        mFakeIdentityManager.removeAccount(TestAccounts.ACCOUNT1.getId());
        verifyNoMoreInteractions(mSyncServiceImplJniMock);
    }

    @Test
    @DisableFeatures(SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS_PART2)
    public void testDestroy_stopsObservingAccounts_legacy() {
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
    @EnableFeatures(SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS_PART2)
    public void testDestroyWhileAccountsNotLoaded_stopsObservingAccounts() {
        mFakeIdentityManager.addOrUpdateExtendedAccountInfo(TestAccounts.ACCOUNT1);
        mFakeIdentityManager.setRefreshTokensLoadingState(RefreshTokensLoadingState.IN_PROGRESS);

        SyncServiceImpl syncService = createSyncService();
        syncService.destroy();

        mFakeIdentityManager.setRefreshTokensLoadingState(RefreshTokensLoadingState.SUCCEEDED);
        verifyNoMoreInteractions(mSyncServiceImplJniMock);
    }

    @Test
    @DisableFeatures(SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS_PART2)
    public void testDestroyWhileAccountsNotLoaded_stopsObservingAccounts_legacy() {
        mFakeAccountManagerFacade.addAccount(TestAccounts.ACCOUNT1);
        try (var ignored =
                mFakeAccountManagerFacade.blockGetAccounts(/* postUnblockCallback= */ null)) {
            SyncServiceImpl syncService = createSyncService();
            syncService.destroy();
        }
        verifyNoMoreInteractions(mSyncServiceImplJniMock);
    }

    private SyncServiceImpl createSyncService() {
        return new SyncServiceImpl(NATIVE_SYNC_SERVICE_ANDROID_BRIDGE, mFakeIdentityManager);
    }
}
