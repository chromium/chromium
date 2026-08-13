// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.metrics;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.regex.Pattern;

/** Unit tests for {@link LimitedEntropyRandomizationSource}. */
@RunWith(BaseRobolectricTestRunner.class)
public class LimitedEntropyRandomizationSourceTest {
    private static final Pattern HEX_PATTERN = Pattern.compile("^[0-9A-F]{32}$");
    private static final String ALL_ZEROS = "00000000000000000000000000000000";

    @Test
    public void testGenerateValue() {
        String value = LimitedEntropyRandomizationSource.generateValue();
        assertNotNull(value);
        assertEquals(32, value.length());
        assertTrue(
                "Value must be a 32-character uppercase hex string",
                HEX_PATTERN.matcher(value).matches());
        assertTrue(
                "ALL_ZEROS must be a 32-character uppercase hex string",
                HEX_PATTERN.matcher(ALL_ZEROS).matches());
        assertFalse("Value must not be all zeros", ALL_ZEROS.equals(value));
    }
}
