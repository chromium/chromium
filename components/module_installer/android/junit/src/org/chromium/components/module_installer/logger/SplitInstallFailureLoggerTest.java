// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.module_installer.logger;

import com.google.android.play.core.splitinstall.model.SplitInstallErrorCode;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;

/** Test suite for the SplitInstallFailureLogger class. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SplitInstallFailureLoggerTest {
    private SplitInstallFailureLogger mFailureLogger;

    private final int mErrorCodeMapping[][] = {
        {4, SplitInstallErrorCode.ACCESS_DENIED},
        {5, SplitInstallErrorCode.ACTIVE_SESSIONS_LIMIT_EXCEEDED},
        {6, SplitInstallErrorCode.API_NOT_AVAILABLE},
        {7, SplitInstallErrorCode.INCOMPATIBLE_WITH_EXISTING_SESSION},
        {8, SplitInstallErrorCode.INSUFFICIENT_STORAGE},
        {9, SplitInstallErrorCode.INVALID_REQUEST},
        {10, SplitInstallErrorCode.MODULE_UNAVAILABLE},
        {11, SplitInstallErrorCode.NETWORK_ERROR},
        {12, SplitInstallErrorCode.NO_ERROR},
        {13, SplitInstallErrorCode.SERVICE_DIED},
        {14, SplitInstallErrorCode.SESSION_NOT_FOUND},
        {15, SplitInstallErrorCode.SPLITCOMPAT_COPY_ERROR},
        {16, SplitInstallErrorCode.SPLITCOMPAT_EMULATION_ERROR},
        {17, SplitInstallErrorCode.SPLITCOMPAT_VERIFICATION_ERROR},
        {18, SplitInstallErrorCode.INTERNAL_ERROR},
    };

    @Before
    public void setUp() {
        mFailureLogger = new SplitInstallFailureLogger();
    }

    @Test
    public void whenLogSuccess_verifyHistogramCode() {
        // Arrange.
        String moduleName = "whenLogSuccess_verifyHistogramCode";
        String histogramName =
                "Android.FeatureModules.InstallStatus.whenLogSuccess_verifyHistogramCode";
        int expectedCode = 0;
        var histogram = HistogramWatcher.newSingleRecordWatcher(histogramName, expectedCode);

        // Act.
        mFailureLogger.logStatusSuccess(moduleName);

        // Assert.
        histogram.assertExpected();
    }

    @Test
    public void whenLogCancelation_verifyHistogramCode() {
        // Arrange.
        String moduleName = "whenLogCancelation_verifyHistogramCode";
        String histogramName =
                "Android.FeatureModules.InstallStatus.whenLogCancelation_verifyHistogramCode";
        int expectedCode = 3;
        var histogram = HistogramWatcher.newSingleRecordWatcher(histogramName, expectedCode);

        // Act.
        mFailureLogger.logStatusCanceled(moduleName);

        // Assert.
        histogram.assertExpected();
    }

    @Test
    public void whenLogNoSplitCompat_verifyHistogramCode() {
        // Arrange.
        String moduleName = "whenLogNoSplitCompat_verifyHistogramCode";
        String histogramName =
                "Android.FeatureModules.InstallStatus.whenLogNoSplitCompat_verifyHistogramCode";
        int expectedCode = 21;
        var histogram = HistogramWatcher.newSingleRecordWatcher(histogramName, expectedCode);

        // Act.
        mFailureLogger.logStatusNoSplitCompat(moduleName);

        // Assert.
        histogram.assertExpected();
    }

    @Test
    public void whenLogStatusFailure_verifyHistogramCode() {
        String moduleName = "whenLogStatusFailure_verifyHistogramCode";
        String histogramName =
                "Android.FeatureModules.InstallStatus.whenLogStatusFailure_verifyHistogramCode";
        int unknownCode = 999;
        int expectedUnknownCode = 19;

        for (int[] tuple : mErrorCodeMapping) {
            // Arrange
            int expectedOutputCode = tuple[0];
            int inputCode = tuple[1];
            var histogram =
                    HistogramWatcher.newSingleRecordWatcher(histogramName, expectedOutputCode);

            // Act
            mFailureLogger.logStatusFailure(moduleName, inputCode);

            // Assert
            histogram.assertExpected();
        }

        // Arrange
        var histogram = HistogramWatcher.newSingleRecordWatcher(histogramName, expectedUnknownCode);

        // Act
        mFailureLogger.logStatusFailure(moduleName, unknownCode);

        // Assert
        histogram.assertExpected();
    }

    @Test
    public void whenLogRequestFailure_verifyHistogramCode() {
        String moduleName = "whenLogRequestFailure_verifyHistogramCode";
        String histogramName =
                "Android.FeatureModules.InstallStatus.whenLogRequestFailure_verifyHistogramCode";
        int unknownCode = 999;
        int expectedUnknownCode = 20;

        for (int[] tuple : mErrorCodeMapping) {
            // Arrange
            int expectedOutputCode = tuple[0];
            int inputCode = tuple[1];
            var histogram =
                    HistogramWatcher.newSingleRecordWatcher(histogramName, expectedOutputCode);

            // Act
            mFailureLogger.logRequestFailure(moduleName, inputCode);

            // Assert
            histogram.assertExpected();
        }

        // Arrange
        var histogram = HistogramWatcher.newSingleRecordWatcher(histogramName, expectedUnknownCode);

        // Act
        mFailureLogger.logRequestFailure(moduleName, unknownCode);

        // Assert
        histogram.assertExpected();
    }
}
