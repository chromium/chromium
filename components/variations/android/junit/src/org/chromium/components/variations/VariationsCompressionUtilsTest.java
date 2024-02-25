// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.variations;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.util.Base64;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.variations.VariationsCompressionUtils.InstanceManipulations;

import java.io.IOException;
import java.nio.charset.StandardCharsets;

/** Tests for VariationsCompressionUtils */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class VariationsCompressionUtilsTest {
    /** Test method for successful {@link VariationsSeedFetcher#getInstanceManipulations} */
    @Test
    public void testGetInstanceManipulations_success()
            throws VariationsCompressionUtils.InvalidImHeaderException {
        InstanceManipulations result =
                VariationsCompressionUtils.getInstanceManipulations("x-bm , gzip");
        assertTrue(result.isDeltaCompressed);
        assertTrue(result.isGzipCompressed);

        result = VariationsCompressionUtils.getInstanceManipulations("x-bm,gzip");
        assertTrue(result.isDeltaCompressed);
        assertTrue(result.isGzipCompressed);

        result = VariationsCompressionUtils.getInstanceManipulations("x-bm");
        assertTrue(result.isDeltaCompressed);
        assertFalse(result.isGzipCompressed);

        result = VariationsCompressionUtils.getInstanceManipulations("gzip");
        assertFalse(result.isDeltaCompressed);
        assertTrue(result.isGzipCompressed);

        result = VariationsCompressionUtils.getInstanceManipulations("");
        assertFalse(result.isDeltaCompressed);
        assertFalse(result.isGzipCompressed);
    }

    @Test(expected = VariationsCompressionUtils.InvalidImHeaderException.class)
    public void testGetInstanceManipulations_invalidOrderThrows()
            throws VariationsCompressionUtils.InvalidImHeaderException {
        InstanceManipulations result =
                VariationsCompressionUtils.getInstanceManipulations("gzip, x-bm");
    }

    @Test(expected = VariationsCompressionUtils.InvalidImHeaderException.class)
    public void testGetInstanceManipulations_invalidEntryThrows()
            throws VariationsCompressionUtils.InvalidImHeaderException {
        InstanceManipulations result =
                VariationsCompressionUtils.getInstanceManipulations("gzip,random-entry,x-bm");
    }

    @Test
    public void testGzipCompression() throws IOException {
        String testString = "Any random data";
        String largeTestString = "";
        for (int i = 0; i < 32 * 1024; i++) {
            largeTestString += "b";
        }

        for (String originalString : new String[] {testString, largeTestString}) {
            byte[] originalBytes = originalString.getBytes(StandardCharsets.UTF_8);

            byte[] gzipCompressed = VariationsCompressionUtils.gzipCompress(originalBytes);
            byte[] gzipUncompressed = VariationsCompressionUtils.gzipUncompress(gzipCompressed);
            String gzipUncompressedString = new String(gzipUncompressed, StandardCharsets.UTF_8);

            assertEquals(originalString, gzipUncompressedString);
        }
    }

    @Test
    public void testDeltaCompression()
            throws IOException, VariationsCompressionUtils.DeltaPatchException {
        String base64BeforeSeedData =
                "CigxN2E4ZGJiOTI4ODI0ZGU3ZDU2MGUyODRlODY1ZDllYzg2NzU1MTE0ElgKDFVNQVN0YWJp"
                        + "bGl0eRjEyomgBTgBQgtTZXBhcmF0ZUxvZ0oLCgdEZWZhdWx0EABKDwoLU2VwYXJhdGVMb2cQ"
                        + "ZFIVEgszNC4wLjE4MDEuMCAAIAEgAiADEkQKIFVNQS1Vbmlmb3JtaXR5LVRyaWFsLTEwMC1Q"
                        + "ZXJjZW50GIDjhcAFOAFCCGdyb3VwXzAxSgwKCGdyb3VwXzAxEAFgARJPCh9VTUEtVW5pZm9y"
                        + "bWl0eS1UcmlhbC01MC1QZXJjZW50GIDjhcAFOAFCB2RlZmF1bHRKDAoIZ3JvdXBfMDEQAUoL"
                        + "CgdkZWZhdWx0EAFgAQ==";
        String base64ExpectedSeedData =
                "CigyNGQzYTM3ZTAxYmViOWYwNWYzMjM4YjUzNWY3MDg1ZmZlZWI4NzQwElgKDFVNQVN0YWJp"
                        + "bGl0eRjEyomgBTgBQgtTZXBhcmF0ZUxvZ0oLCgdEZWZhdWx0EABKDwoLU2VwYXJhdGVMb2cQ"
                        + "ZFIVEgszNC4wLjE4MDEuMCAAIAEgAiADEpIBCh9VTUEtVW5pZm9ybWl0eS1UcmlhbC0yMC1Q"
                        + "ZXJjZW50GIDjhcAFOAFCB2RlZmF1bHRKEQoIZ3JvdXBfMDEQARijtskBShEKCGdyb3VwXzAy"
                        + "EAEYpLbJAUoRCghncm91cF8wMxABGKW2yQFKEQoIZ3JvdXBfMDQQARimtskBShAKB2RlZmF1"
                        + "bHQQARiitskBYAESWAofVU1BLVVuaWZvcm1pdHktVHJpYWwtNTAtUGVyY2VudBiA44XABTgB"
                        + "QgdkZWZhdWx0Sg8KC25vbl9kZWZhdWx0EAFKCwoHZGVmYXVsdBABUgQoACgBYAE=";
        String base64DeltaPatch =
                "KgooMjRkM2EzN2UwMWJlYjlmMDVmMzIzOGI1MzVmNzA4NWZmZWViODc0MAAqW+4BkgEKH1VN"
                        + "QS1Vbmlmb3JtaXR5LVRyaWFsLTIwLVBlcmNlbnQYgOOFwAU4AUIHZGVmYXVsdEoRCghncm91"
                        + "cF8wMRABGKO2yQFKEQoIZ3JvdXBfMDIQARiktskBShEKCGdyb3VwXzAzEAEYpbbJAUoRCghn"
                        + "cm91cF8wNBABGKa2yQFKEAoHZGVmYXVsdBABGKK2yQFgARJYCh9VTUEtVW5pZm9ybWl0eS1U"
                        + "cmlhbC01MC1QZXJjZW50GIDjhcAFOAFCB2RlZmF1bHRKDwoLbm9uX2RlZmF1bHQQAUoLCgdk"
                        + "ZWZhdWx0EAFSBCgAKAFgAQ==";

        byte[] beforeSeedData = Base64.decode(base64BeforeSeedData, Base64.NO_WRAP);
        byte[] deltaPatch = Base64.decode(base64DeltaPatch, Base64.NO_WRAP);

        byte[] byteResult = VariationsCompressionUtils.applyDeltaPatch(beforeSeedData, deltaPatch);

        assertEquals(
                "Delta patched seed data should result in expectedSeedData",
                base64ExpectedSeedData,
                Base64.encodeToString(byteResult, Base64.NO_WRAP));
    }
}
