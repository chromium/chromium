// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.trusted_vault;

import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Promise;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.components.signin.test.util.TestAccounts;

import java.util.Collections;
import java.util.List;

/** Unit tests for {@link TrustedVaultClient}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TrustedVaultClientTest {
    private static final long NATIVE_TRUSTED_VAULT_CLIENT_ANDROID = 12345L;
    private static final int REQUEST_ID = 7;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TrustedVaultClient.Natives mNativesMock;
    @Mock private TrustedVaultClient.Backend mBackendMock;

    @Before
    public void setUp() {
        TrustedVaultClientJni.setInstanceForTesting(mNativesMock);
    }

    @Test
    public void testFetchKeys_Fulfilled() {
        byte[] key1 = new byte[] {1, 2, 3};
        byte[] key2 = new byte[] {4, 5, 6};
        when(mBackendMock.fetchKeysAndVersions(TestAccounts.ACCOUNT1))
                .thenReturn(
                        Promise.fulfilled(
                                List.of(
                                        new TrustedVaultKeyAndVersion(key1, 2),
                                        new TrustedVaultKeyAndVersion(key2, 5))));
        TrustedVaultClient.get(SecurityDomainId.CHROME_SYNC).setBackendForTesting(mBackendMock);
        TrustedVaultClient.registerNative(
                NATIVE_TRUSTED_VAULT_CLIENT_ANDROID, SecurityDomainId.CHROME_SYNC);
        ResettersForTesting.register(
                () ->
                        TrustedVaultClient.unregisterNative(
                                NATIVE_TRUSTED_VAULT_CLIENT_ANDROID, SecurityDomainId.CHROME_SYNC));

        TrustedVaultClient.fetchKeys(
                NATIVE_TRUSTED_VAULT_CLIENT_ANDROID,
                SecurityDomainId.CHROME_SYNC,
                REQUEST_ID,
                TestAccounts.ACCOUNT1);
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mNativesMock)
                .fetchKeysCompleted(
                        NATIVE_TRUSTED_VAULT_CLIENT_ANDROID,
                        REQUEST_ID,
                        TestAccounts.ACCOUNT1.getGaiaId().toString(),
                        List.of(
                                new TrustedVaultKeyAndVersion(key1, 2),
                                new TrustedVaultKeyAndVersion(key2, 5)));
    }

    @Test
    public void testFetchKeys_Rejected() {
        when(mBackendMock.fetchKeysAndVersions(TestAccounts.ACCOUNT1))
                .thenReturn(Promise.rejected());
        TrustedVaultClient.get(SecurityDomainId.CHROME_SYNC).setBackendForTesting(mBackendMock);
        TrustedVaultClient.registerNative(
                NATIVE_TRUSTED_VAULT_CLIENT_ANDROID, SecurityDomainId.CHROME_SYNC);
        ResettersForTesting.register(
                () ->
                        TrustedVaultClient.unregisterNative(
                                NATIVE_TRUSTED_VAULT_CLIENT_ANDROID, SecurityDomainId.CHROME_SYNC));

        TrustedVaultClient.fetchKeys(
                NATIVE_TRUSTED_VAULT_CLIENT_ANDROID,
                SecurityDomainId.CHROME_SYNC,
                REQUEST_ID,
                TestAccounts.ACCOUNT1);
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mNativesMock)
                .fetchKeysCompleted(
                        NATIVE_TRUSTED_VAULT_CLIENT_ANDROID,
                        REQUEST_ID,
                        TestAccounts.ACCOUNT1.getGaiaId().toString(),
                        Collections.emptyList());
    }
}
