// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.module_installer.logger;

import com.google.android.play.core.splitinstall.model.SplitInstallSessionStatus;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;

/**
 * Test suite for the SplitInstallStatusLogger class.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SplitInstallStatusLoggerTest {
    private SplitInstallStatusLogger mStatusLogger;

    @Before
    public void setUp() {
        mStatusLogger = new SplitInstallStatusLogger();
    }

    @Test
    public void whenLogRequestStart_verifyHistogramCode() {
        // Arrange.
        String moduleName = "whenLogRequestStart_verifyHistogramCode";
        String histogramName =
                "Android.FeatureModules.InstallingStatus.whenLogRequestStart_verifyHistogramCode";
        int expectedCode = 1;
        var histogram = HistogramWatcher.newSingleRecordWatcher(histogramName, expectedCode);

        // Act.
        mStatusLogger.logRequestStart(moduleName);

        // Assert.
        histogram.assertExpected();
    }

    @Test
    public void whenLogRequestDeferredStart_verifyHistogramCode() {
        // Arrange.
        String moduleName = "whenLogRequestDeferredStart_verifyHistogramCode";
        String histogramName = "Android.FeatureModules.InstallingStatus."
                + "whenLogRequestDeferredStart_verifyHistogramCode";
        int expectedCode = 11;
        var histogram = HistogramWatcher.newSingleRecordWatcher(histogramName, expectedCode);

        // Act.
        mStatusLogger.logRequestDeferredStart(moduleName);

        // Assert.
        histogram.assertExpected();
    }

    @Test
    public void whenLogStatusChange_verifyHistogramCode() {
        String moduleName = "whenLogStatusChange_verifyHistogramCode";
        String histogramName =
                "Android.FeatureModules.InstallingStatus.whenLogStatusChange_verifyHistogramCode";
        int unknownCode = 999;

        doTestStatusChange(histogramName, 0, moduleName, unknownCode);
        doTestStatusChange(histogramName, 2, moduleName, SplitInstallSessionStatus.PENDING);
        doTestStatusChange(histogramName, 3, moduleName, SplitInstallSessionStatus.DOWNLOADING);
        doTestStatusChange(histogramName, 4, moduleName, SplitInstallSessionStatus.DOWNLOADED);
        doTestStatusChange(histogramName, 5, moduleName, SplitInstallSessionStatus.INSTALLING);
        doTestStatusChange(histogramName, 6, moduleName, SplitInstallSessionStatus.INSTALLED);
        doTestStatusChange(histogramName, 7, moduleName, SplitInstallSessionStatus.FAILED);
        doTestStatusChange(histogramName, 8, moduleName, SplitInstallSessionStatus.CANCELING);
        doTestStatusChange(histogramName, 9, moduleName, SplitInstallSessionStatus.CANCELED);
        doTestStatusChange(histogramName, 10, moduleName,
                SplitInstallSessionStatus.REQUIRES_USER_CONFIRMATION);
    }

    private void doTestStatusChange(String histogramName, int expectedEnumValue, String moduleName,
            @SplitInstallSessionStatus int status) {
        // Arrange
        var histogram = HistogramWatcher.newSingleRecordWatcher(histogramName, expectedEnumValue);

        // Act
        mStatusLogger.logStatusChange(moduleName, status);

        // Assert
        histogram.assertExpected();
    }
}
