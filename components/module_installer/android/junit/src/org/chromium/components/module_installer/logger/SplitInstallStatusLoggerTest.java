// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.module_installer.logger;

import static org.junit.Assert.assertEquals;

import com.google.android.play.core.splitinstall.model.SplitInstallSessionStatus;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.metrics.UmaRecorderHolder;
import org.chromium.base.test.BaseRobolectricTestRunner;

/**
 * Test suite for the SplitInstallStatusLogger class.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SplitInstallStatusLoggerTest {
    private SplitInstallStatusLogger mStatusLogger;

    @Before
    public void setUp() {
        UmaRecorderHolder.resetForTesting();
        mStatusLogger = new SplitInstallStatusLogger();
    }

    @Test
    public void whenLogRequestStart_verifyHistogramCode() {
        // Arrange.
        String moduleName = "whenLogRequestStart_verifyHistogramCode";
        int expectedCode = 1;

        // Act.
        mStatusLogger.logRequestStart(moduleName);

        // Assert.
        assertEquals(expectedCode, getHistogramStatus(moduleName));
    }

    @Test
    public void whenLogRequestDeferredStart_verifyHistogramCode() {
        // Arrange.
        String moduleName = "whenLogRequestDeferredStart_verifyHistogramCode";
        int expectedCode = 11;

        // Act.
        mStatusLogger.logRequestDeferredStart(moduleName);

        // Assert.
        assertEquals(expectedCode, getHistogramStatus(moduleName));
    }

    @Test
    public void whenLogStatusChange_verifyHistogramCode() {
        // Arrange.
        String moduleName = "whenLogStatusChange_verifyHistogramCode";
        int unknownCode = 999;

        // Act & Assert.
        assertEquals(0, logStatusChange(moduleName, unknownCode));
        assertEquals(2, logStatusChange(moduleName, SplitInstallSessionStatus.PENDING));
        assertEquals(3, logStatusChange(moduleName, SplitInstallSessionStatus.DOWNLOADING));
        assertEquals(4, logStatusChange(moduleName, SplitInstallSessionStatus.DOWNLOADED));
        assertEquals(5, logStatusChange(moduleName, SplitInstallSessionStatus.INSTALLING));
        assertEquals(6, logStatusChange(moduleName, SplitInstallSessionStatus.INSTALLED));
        assertEquals(7, logStatusChange(moduleName, SplitInstallSessionStatus.FAILED));
        assertEquals(8, logStatusChange(moduleName, SplitInstallSessionStatus.CANCELING));
        assertEquals(9, logStatusChange(moduleName, SplitInstallSessionStatus.CANCELED));
        assertEquals(10,
                logStatusChange(moduleName, SplitInstallSessionStatus.REQUIRES_USER_CONFIRMATION));
    }

    private int logStatusChange(String moduleName, int status) {
        UmaRecorderHolder.resetForTesting();
        mStatusLogger.logStatusChange(moduleName, status);
        return getHistogramStatus(moduleName);
    }

    private int getHistogramStatus(String moduleName) {
        String expName = "Android.FeatureModules.InstallingStatus." + moduleName;
        Integer expBoundary = 12;
        return LoggerTestUtil.getHistogramStatus(expName, expBoundary);
    }
}
