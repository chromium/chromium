// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.language;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.ServiceLoaderUtil;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.Collections;
import java.util.List;
import java.util.concurrent.TimeoutException;

/** Tests for LanguageProfileController. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class LanguageProfileControllerUnitTest {
    @Before
    public void setUp() {
        ThreadUtils.hasSubtleSideEffectsSetThreadAssertsDisabledForTesting(true);
    }

    @Test
    @SmallTest
    public void testSuccess() {
        ServiceLoaderUtil.setInstanceForTesting(
                LanguageProfileDelegate.class,
                new LanguageProfileDelegate() {
                    @Override
                    public List<String> getLanguagePreferences(
                            String accountName, int timeoutInSeconds) {
                        return Collections.emptyList();
                    }
                });

        LanguageProfileController.getLanguagePreferences("myaccount");

        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        LanguageProfileMetricsLogger.INITIATION_STATUS_HISTOGRAM));
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        LanguageProfileMetricsLogger.INITIATION_STATUS_HISTOGRAM,
                        LanguageProfileMetricsLogger.ULPInitiationStatus.SUCCESS));
        // Ensure that only the signed-in histograms are populated.
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        LanguageProfileMetricsLogger.SIGNED_IN_INITIATION_STATUS_HISTOGRAM));
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        LanguageProfileMetricsLogger.SIGNED_IN_INITIATION_STATUS_HISTOGRAM,
                        LanguageProfileMetricsLogger.ULPInitiationStatus.SUCCESS));
        Assert.assertEquals(
                0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        LanguageProfileMetricsLogger.SIGNED_OUT_INITIATION_STATUS_HISTOGRAM));
    }

    @Test
    @SmallTest
    public void testSignedOut() {
        ServiceLoaderUtil.setInstanceForTesting(
                LanguageProfileDelegate.class,
                new LanguageProfileDelegate() {
                    @Override
                    public List<String> getLanguagePreferences(
                            String accountName, int timeoutInSeconds) {
                        return Collections.emptyList();
                    }
                });

        LanguageProfileController.getLanguagePreferences(null);

        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        LanguageProfileMetricsLogger.INITIATION_STATUS_HISTOGRAM));
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        LanguageProfileMetricsLogger.INITIATION_STATUS_HISTOGRAM,
                        LanguageProfileMetricsLogger.ULPInitiationStatus.SUCCESS));
        // Ensure that only the signed-out histograms are populated.
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        LanguageProfileMetricsLogger.SIGNED_OUT_INITIATION_STATUS_HISTOGRAM));
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        LanguageProfileMetricsLogger.SIGNED_OUT_INITIATION_STATUS_HISTOGRAM,
                        LanguageProfileMetricsLogger.ULPInitiationStatus.SUCCESS));
        Assert.assertEquals(
                0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        LanguageProfileMetricsLogger.SIGNED_IN_INITIATION_STATUS_HISTOGRAM));
    }

    @Test
    @SmallTest
    public void testNotAvailable() {
        LanguageProfileController.getLanguagePreferences("myaccount");

        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        LanguageProfileMetricsLogger.INITIATION_STATUS_HISTOGRAM));
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        LanguageProfileMetricsLogger.INITIATION_STATUS_HISTOGRAM,
                        LanguageProfileMetricsLogger.ULPInitiationStatus.NOT_SUPPORTED));
    }

    @Test
    @SmallTest
    public void testTimeout() {
        ServiceLoaderUtil.setInstanceForTesting(
                LanguageProfileDelegate.class,
                new LanguageProfileDelegate() {
                    @Override
                    public List<String> getLanguagePreferences(
                            String accountName, int timeoutInSeconds) throws TimeoutException {
                        throw new TimeoutException("error!");
                    }
                });

        LanguageProfileController.getLanguagePreferences("myaccount");

        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        LanguageProfileMetricsLogger.INITIATION_STATUS_HISTOGRAM));
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        LanguageProfileMetricsLogger.INITIATION_STATUS_HISTOGRAM,
                        LanguageProfileMetricsLogger.ULPInitiationStatus.TIMED_OUT));
    }

    @Test
    @SmallTest
    public void testFailure() {
        ServiceLoaderUtil.setInstanceForTesting(
                LanguageProfileDelegate.class,
                new LanguageProfileDelegate() {
                    @Override
                    public List<String> getLanguagePreferences(
                            String accountName, int timeoutInSeconds) throws InterruptedException {
                        throw new InterruptedException("error!");
                    }
                });

        LanguageProfileController.getLanguagePreferences("myaccount");

        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        LanguageProfileMetricsLogger.INITIATION_STATUS_HISTOGRAM));
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        LanguageProfileMetricsLogger.INITIATION_STATUS_HISTOGRAM,
                        LanguageProfileMetricsLogger.ULPInitiationStatus.FAILURE));
    }
}
