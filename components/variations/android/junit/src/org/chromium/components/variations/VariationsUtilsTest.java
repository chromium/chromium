// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.variations;

import static org.junit.Assert.assertEquals;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.metrics.test.ShadowRecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;

import java.io.IOException;
import java.nio.charset.StandardCharsets;

/**
 * Tests for VariationsUtils
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowRecordHistogram.class})
public class VariationsUtilsTest {
    @Test
    public void testGzipCompression() throws IOException {
        String testString = "Any random data";
        String largeTestString = "";
        for (int i = 0; i < 32 * 1024; i++) {
            largeTestString += "b";
        }

        for (String originalString : new String[] {testString, largeTestString}) {
            byte[] originalBytes = originalString.getBytes(StandardCharsets.UTF_8);

            byte[] gzipCompressed = VariationsUtils.gzipCompress(originalBytes);
            byte[] gzipUncompressed = VariationsUtils.gzipUncompress(gzipCompressed);
            String gzipUncompressedString = new String(gzipUncompressed, StandardCharsets.UTF_8);

            assertEquals(originalString, gzipUncompressedString);
        }
    }
}
