// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.trusted_vault;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link TrustedVaultKeyAndVersion}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TrustedVaultKeyAndVersionTest {
    @Test
    public void testEqualsAndHashCode() {
        TrustedVaultKeyAndVersion key1 = new TrustedVaultKeyAndVersion(new byte[] {1, 2, 3}, 5);
        TrustedVaultKeyAndVersion key1Copy = new TrustedVaultKeyAndVersion(new byte[] {1, 2, 3}, 5);
        TrustedVaultKeyAndVersion differentKey =
                new TrustedVaultKeyAndVersion(new byte[] {1, 2, 4}, 5);
        TrustedVaultKeyAndVersion differentVersion =
                new TrustedVaultKeyAndVersion(new byte[] {1, 2, 3}, 6);

        assertEquals(key1, key1Copy);
        assertEquals(key1.hashCode(), key1Copy.hashCode());
        assertNotEquals(key1, differentKey);
        assertNotEquals(key1, differentVersion);
        assertNotEquals(key1, null);
    }
}
