// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.module_installer.logger;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.atLeast;
import static org.mockito.Mockito.verify;

import org.mockito.ArgumentCaptor;

import org.chromium.base.metrics.CachedMetrics;
import org.chromium.base.metrics.RecordHistogram;

/**
 * Util class for supporting logger testing.
 */
public class LoggerTestUtil {
    public static int getHistogramStatus(
            RecordHistogram.Natives mockHistogram, String expectedName, Integer expectedBoundary) {
        ArgumentCaptor<String> name = ArgumentCaptor.forClass(String.class);
        ArgumentCaptor<Long> key = ArgumentCaptor.forClass(Long.class);
        ArgumentCaptor<Integer> sample = ArgumentCaptor.forClass(Integer.class);
        ArgumentCaptor<Integer> boundary = ArgumentCaptor.forClass(Integer.class);

        // Make sure the metrics are flushed.
        // Needed by the EnumeratedHistogramSample but not for RecordHistogram.
        CachedMetrics.commitCachedMetrics();

        verify(mockHistogram, atLeast(1))
                .recordEnumeratedHistogram(
                        name.capture(), key.capture(), sample.capture(), boundary.capture());

        assertEquals(expectedName, name.getValue());
        assertEquals(expectedBoundary, boundary.getValue());

        return sample.getValue();
    }
}