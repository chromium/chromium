// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.password_manager.ode;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Promise;
import org.chromium.base.ServiceLoaderUtil;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.components.trusted_vault.SecurityDomainId;
import org.chromium.components.trusted_vault.TrustedVaultClient;

import java.util.concurrent.atomic.AtomicReference;

/** Unit tests for {@link PasskeySecurityDomainChecker}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PasskeySecurityDomainCheckerTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TrustedVaultClient.BackendFactory mBackendFactory;
    @Mock private TrustedVaultClient.Backend mBackend;

    @Before
    public void setUp() {
        when(mBackendFactory.createBackend(SecurityDomainId.PASSKEYS)).thenReturn(mBackend);
        ServiceLoaderUtil.setInstanceForTesting(
                TrustedVaultClient.BackendFactory.class, mBackendFactory);
    }

    @Test
    public void testGetIsSecurityDomainEmpty_NoBackendFactoryReturnsNull() {
        ServiceLoaderUtil.setInstanceForTesting(TrustedVaultClient.BackendFactory.class, null);

        AtomicReference<Boolean> result = new AtomicReference<>(true);
        PasskeySecurityDomainChecker.getIsSecurityDomainEmpty(TestAccounts.ACCOUNT1, result::set);

        assertNull(result.get());
    }

    @Test
    public void testGetIsSecurityDomainEmpty_ReturnsTrue() {
        when(mBackend.getIsSecurityDomainEmpty(TestAccounts.ACCOUNT1))
                .thenReturn(Promise.fulfilled(true));

        AtomicReference<Boolean> result = new AtomicReference<>();
        PasskeySecurityDomainChecker.getIsSecurityDomainEmpty(TestAccounts.ACCOUNT1, result::set);
        ShadowLooper.idleMainLooper();

        assertTrue(result.get());
    }

    @Test
    public void testGetIsSecurityDomainEmpty_ReturnsFalse() {
        when(mBackend.getIsSecurityDomainEmpty(TestAccounts.ACCOUNT1))
                .thenReturn(Promise.fulfilled(false));

        AtomicReference<Boolean> result = new AtomicReference<>();
        PasskeySecurityDomainChecker.getIsSecurityDomainEmpty(TestAccounts.ACCOUNT1, result::set);
        ShadowLooper.idleMainLooper();

        assertFalse(result.get());
    }

    @Test
    public void testGetIsSecurityDomainEmpty_RejectedReturnsNull() {
        when(mBackend.getIsSecurityDomainEmpty(TestAccounts.ACCOUNT1))
                .thenReturn(Promise.rejected());

        AtomicReference<Boolean> result = new AtomicReference<>(true);
        PasskeySecurityDomainChecker.getIsSecurityDomainEmpty(TestAccounts.ACCOUNT1, result::set);
        ShadowLooper.idleMainLooper();

        assertNull(result.get());
    }
}
