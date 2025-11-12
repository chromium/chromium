// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.omnibox;

import static org.junit.Assert.assertEquals;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.security_state.ConnectionMaliciousContentStatus;
import org.chromium.components.security_state.ConnectionSecurityLevel;

/** Unit tests for {@link SecurityStatusIcon}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SecurityStatusIconUnitTest {

    @Test
    public void testGetSecurityIconResource_None_SmallDevice_SkipIcon() {
        int iconResource =
                SecurityStatusIcon.getSecurityIconResource(
                        ConnectionSecurityLevel.NONE,
                        () -> ConnectionMaliciousContentStatus.NONE,
                        /* isSmallDevice= */ true,
                        /* skipIconForNeutralState= */ true,
                        /* useLockIconForSecureState= */ false);
        assertEquals(0, iconResource);
    }

    @Test
    public void testGetSecurityIconResource_Secure_LockIcon() {
        int iconResource =
                SecurityStatusIcon.getSecurityIconResource(
                        ConnectionSecurityLevel.SECURE,
                        () -> ConnectionMaliciousContentStatus.NONE,
                        /* isSmallDevice= */ false,
                        /* skipIconForNeutralState= */ false,
                        /* useLockIconForSecureState= */ true);
        assertEquals(R.drawable.omnibox_https_valid_lock, iconResource);
    }

    @Test
    public void testGetSecurityIconResource_None_Info() {
        assertIconResourceIs(
                R.drawable.omnibox_info,
                ConnectionSecurityLevel.NONE,
                ConnectionMaliciousContentStatus.NONE);
    }

    @Test
    public void testGetSecurityIconResource_Secure_PageInfo() {
        assertIconResourceIs(
                R.drawable.omnibox_https_valid_page_info,
                ConnectionSecurityLevel.SECURE,
                ConnectionMaliciousContentStatus.NONE);
    }

    @Test
    public void testGetSecurityIconResource_Warning() {
        assertIconResourceIs(
                R.drawable.omnibox_not_secure_warning,
                ConnectionSecurityLevel.WARNING,
                ConnectionMaliciousContentStatus.NONE);
    }

    @Test
    public void testGetSecurityIconResource_Dangerous() {
        assertIconResourceIs(
                R.drawable.omnibox_dangerous,
                ConnectionSecurityLevel.DANGEROUS,
                ConnectionMaliciousContentStatus.NONE);
    }

    @Test
    public void testGetSecurityIconResource_Dangerous_ManagedPolicyWarn() {
        assertIconResourceIs(
                R.drawable.enterprise_management,
                ConnectionSecurityLevel.DANGEROUS,
                ConnectionMaliciousContentStatus.MANAGED_POLICY_WARN);
    }

    @Test
    public void testGetSecurityIconResource_Dangerous_ManagedPolicyBlock() {
        assertIconResourceIs(
                R.drawable.enterprise_management,
                ConnectionSecurityLevel.DANGEROUS,
                ConnectionMaliciousContentStatus.MANAGED_POLICY_BLOCK);
    }

    @Test
    public void testGetSecurityIconResource_Dangerous_Billing() {
        assertIconResourceIs(
                R.drawable.omnibox_not_secure_warning,
                ConnectionSecurityLevel.DANGEROUS,
                ConnectionMaliciousContentStatus.BILLING);
    }

    private static void assertIconResourceIs(
            int expectedIconResource,
            @ConnectionSecurityLevel int securityLevel,
            @ConnectionMaliciousContentStatus int maliciousContentStatus) {
        int iconResource =
                SecurityStatusIcon.getSecurityIconResource(
                        securityLevel,
                        () -> maliciousContentStatus,
                        /* isSmallDevice= */ false,
                        /* skipIconForNeutralState= */ false,
                        /* useLockIconForSecureState= */ false);
        assertEquals(expectedIconResource, iconResource);
    }
}
