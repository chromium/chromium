// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.segmentation_platform;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Tests for {@link ProcessedValue}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ProcessedValueTest {
    @Test
    public void testEqualsAndHashCode_Boolean() {
        ProcessedValue processedValue1 = ProcessedValue.fromBoolean(true);
        ProcessedValue processedValue2 = ProcessedValue.fromBoolean(true);
        ProcessedValue processedValue3 = ProcessedValue.fromBoolean(false);

        testEqualsAndHashCodeImpl(processedValue1, processedValue2, processedValue3);
    }

    @Test
    public void testEqualsAndHashCode_Int() {
        ProcessedValue processedValue1 = ProcessedValue.fromInt(1);
        ProcessedValue processedValue2 = ProcessedValue.fromInt(1);
        ProcessedValue processedValue3 = ProcessedValue.fromInt(2);

        testEqualsAndHashCodeImpl(processedValue1, processedValue2, processedValue3);
    }

    @Test
    public void testEqualsAndHashCode_Float() {
        ProcessedValue processedValue1 = ProcessedValue.fromFloat(1.0f);
        ProcessedValue processedValue2 = ProcessedValue.fromFloat(1.0f);
        ProcessedValue processedValue3 = ProcessedValue.fromFloat(2.0f);

        testEqualsAndHashCodeImpl(processedValue1, processedValue2, processedValue3);
    }

    @Test
    public void testEqualsAndHashCode_Double() {
        ProcessedValue processedValue1 = ProcessedValue.fromDouble(1.0);
        ProcessedValue processedValue2 = ProcessedValue.fromDouble(1.0);
        ProcessedValue processedValue3 = ProcessedValue.fromDouble(2.0);

        testEqualsAndHashCodeImpl(processedValue1, processedValue2, processedValue3);
    }

    @Test
    public void testEqualsAndHashCode_String() {
        ProcessedValue processedValue1 = ProcessedValue.fromString("hello");
        ProcessedValue processedValue2 = ProcessedValue.fromString("hello");
        ProcessedValue processedValue3 = ProcessedValue.fromString("world");
        testEqualsAndHashCodeImpl(processedValue1, processedValue2, processedValue3);
    }

    @Test
    public void testEqualsAndHashCode_TimeMillis() {
        long time1 = 1000L;
        long time2 = 2000L;
        ProcessedValue processedValue1 = ProcessedValue.fromTimeMillis(time1);
        ProcessedValue processedValue2 = ProcessedValue.fromTimeMillis(time1);
        ProcessedValue processedValue3 = ProcessedValue.fromTimeMillis(time2);

        testEqualsAndHashCodeImpl(processedValue1, processedValue2, processedValue3);
    }

    @Test
    public void testEqualsAndHashCode_Int64() {
        long val1 = 10000000000L;
        long val2 = 20000000000L;
        ProcessedValue processedValue1 = ProcessedValue.fromInt64(val1);
        ProcessedValue processedValue2 = ProcessedValue.fromInt64(val1);
        ProcessedValue processedValue3 = ProcessedValue.fromInt64(val2);

        testEqualsAndHashCodeImpl(processedValue1, processedValue2, processedValue3);
    }

    @Test
    public void testEqualsAndHashCode_GURL() {
        GURL url = JUnitTestGURLs.EXAMPLE_URL;
        GURL url2 = JUnitTestGURLs.GOOGLE_URL;
        ProcessedValue processedValue1 = ProcessedValue.fromGURL(url);
        ProcessedValue processedValue2 = ProcessedValue.fromGURL(new GURL(url.getSpec()));
        ProcessedValue processedValue3 = ProcessedValue.fromGURL(url2);
        testEqualsAndHashCodeImpl(processedValue1, processedValue2, processedValue3);
    }

    private void testEqualsAndHashCodeImpl(
            ProcessedValue processedValue1,
            ProcessedValue processedValue2,
            ProcessedValue processedValue3) {
        assertTrue(processedValue1.equals(processedValue2));
        assertEquals(processedValue1.hashCode(), processedValue2.hashCode());

        assertFalse(processedValue1.equals(null));
        assertFalse(processedValue1.equals(processedValue3));

        ProcessedValue differentTypeProcessedValue;
        if (processedValue1.type != ProcessedValueType.INT) {
            differentTypeProcessedValue = ProcessedValue.fromInt(999);
        } else {
            differentTypeProcessedValue = ProcessedValue.fromBoolean(false);
        }
        assertFalse(processedValue1.equals(differentTypeProcessedValue));
    }
}
