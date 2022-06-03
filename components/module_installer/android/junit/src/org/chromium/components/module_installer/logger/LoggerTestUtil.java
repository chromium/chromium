// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.module_installer.logger;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.greaterThan;

import org.chromium.base.metrics.RecordHistogram;

/**
 * Util class for supporting logger testing.
 *
 * TODO(bttk): remove in favor of a general purpose metrics test util
 */
public class LoggerTestUtil {
    /**
     * Asserts that enumerated histogram {@code name} has at least one sample.
     *
     * @param name Name of the enumerated histogram.
     * @param boundary The smallest value not in the enumeration.
     * @return The smallest recorded sample.
     */
    public static int getHistogramStatus(String name, int boundary) {
        int sampleCount = RecordHistogram.getHistogramTotalCountForTesting(name);
        assertThat(sampleCount, greaterThan(0));

        for (int i = 0; i < boundary; i++) {
            if (RecordHistogram.getHistogramValueCountForTesting(name, i) > 0) {
                return i;
            }
        }
        // There are samples only in the overflow bucket. More context:
        // https://chromium.googlesource.com/chromium/src.git/+/HEAD/tools/metrics/histograms/README.md#count-histograms_choosing-min-and-max
        return Integer.MAX_VALUE;
    }
}
