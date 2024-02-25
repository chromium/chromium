// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.installedapp;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.BrowserContextHandle;

import java.util.Arrays;

/** Tests that PackageHash generates correct hashes. */
@RunWith(BaseRobolectricTestRunner.class)
public class PackageHashTest {
    private static class FakeBrowserContextHandle implements BrowserContextHandle {
        @Override
        public long getNativeBrowserContextPointer() {
            return 1;
        }
    }

    @Test
    @Feature({"InstalledApp"})
    public void testPackageHash() {
        byte[] salt = {
            0x64, 0x09, -0x68, -0x25, 0x70, 0x11, 0x25, 0x24, 0x68, -0x1a, 0x08, 0x79, -0x12, -0x50,
            0x3b, -0x57, -0x17, -0x4d, 0x46, 0x02
        };
        PackageHash.setGlobalSaltForTesting(salt);
        // These expectations are based on the salt + ':' + packageName, encoded in UTF-8, hashed
        // with SHA-256, and looking at the first two bytes of the result.
        Assert.assertEquals((short) 0x0d4d, PackageHash.hashForPackage("com.example.test1", null));
        Assert.assertEquals(
                (short) 0xfa6f, PackageHash.hashForPackage("com.example.t\u00e9st2", null));

        byte[] salt2 = {
            -0x10, 0x38, -0x28, 0x1f, 0x59, 0x2d, -0x2d, -0x4a, 0x23, 0x76, 0x6d, -0x54, 0x27,
            -0x2d, -0x3f, -0x59, -0x2e, -0x0e, 0x67, 0x7a
        };
        PackageHash.setGlobalSaltForTesting(salt2);
        Assert.assertEquals((short) 0xd6d6, PackageHash.hashForPackage("com.example.test1", null));
        Assert.assertEquals(
                (short) 0x5193, PackageHash.hashForPackage("com.example.t\u00e9st2", null));
    }

    @Test
    @Feature({"InstalledApp"})
    public void testBrowsingSessionSalts() {
        BrowserContextHandle firstHandle = new FakeBrowserContextHandle();
        byte[] firstSalt = PackageHash.getSaltBytes(firstHandle);
        // Verify the same salt is given a second time.
        Assert.assertArrayEquals(firstSalt, PackageHash.getSaltBytes(firstHandle));

        // Verify a different salt is returned for a different browsing session.
        BrowserContextHandle secondHandle = new FakeBrowserContextHandle();
        byte[] secondSalt = PackageHash.getSaltBytes(secondHandle);
        Assert.assertFalse(Arrays.equals(firstSalt, secondSalt));

        // The salt should have been updated after cookies were deleted.
        PackageHash.onCookiesDeleted(firstHandle);
        byte[] postDeletionSalt = PackageHash.getSaltBytes(firstHandle);
        Assert.assertFalse(Arrays.equals(firstSalt, postDeletionSalt));

        // But not for the other browsing session.
        Assert.assertArrayEquals(secondSalt, PackageHash.getSaltBytes(secondHandle));
    }
}
