// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.module_installer.logger;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.verify;

import com.google.android.play.core.splitinstall.model.SplitInstallSessionStatus;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Test suite for the PlayCoreLogger class. */
@RunWith(BaseRobolectricTestRunner.class)
public class PlayCoreLoggerTest {
    @Mock private SplitInstallFailureLogger mFailureLogger;

    @Mock private SplitInstallStatusLogger mStatusLogger;

    @Mock private SplitAvailabilityLogger mAvailabilityLogger;

    private PlayCoreLogger mPlayCoreLogger;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mPlayCoreLogger = new PlayCoreLogger(mFailureLogger, mStatusLogger, mAvailabilityLogger);
    }

    @Test
    public void whenLogRequestFailure_verifyLogged() {
        // Arrange.
        String moduleName = "whenLogRequestFailure_verifyLogged";
        Integer errorCode = 1;

        // Act.
        mPlayCoreLogger.logRequestFailure(moduleName, errorCode);

        // Assert.
        verify(mFailureLogger).logRequestFailure(moduleName, errorCode);
    }

    @Test
    public void whenLogStatusFailure_verifyLogged() {
        // Arrange.
        String moduleName = "whenLogStatusFailure_verifyLogged";
        Integer errorCode = 1;

        // Act.
        mPlayCoreLogger.logStatusFailure(moduleName, errorCode);

        // Assert.
        verify(mFailureLogger).logStatusFailure(moduleName, errorCode);
    }

    @Test
    public void whenLogInstalledStatus_verifyLogged() {
        // Arrange.
        String moduleName = "whenLogInstalledStatus_verifyLogged";
        Integer status = SplitInstallSessionStatus.INSTALLED;
        InOrder inOrder = inOrder(mStatusLogger, mAvailabilityLogger, mFailureLogger);

        // Act.
        mPlayCoreLogger.logStatus(moduleName, status);

        // Assert.
        inOrder.verify(mStatusLogger).logStatusChange(moduleName, status);
        inOrder.verify(mAvailabilityLogger).storeModuleInstalled(moduleName, status);
        inOrder.verify(mAvailabilityLogger).logInstallTimes(moduleName);
        inOrder.verify(mFailureLogger).logStatusSuccess(moduleName);
        inOrder.verifyNoMoreInteractions();
    }

    @Test
    public void whenLogCanceledStatus_verifyLogged() {
        // Arrange.
        String moduleName = "whenLogCanceledStatus_verifyLogged";
        Integer status = SplitInstallSessionStatus.CANCELED;
        InOrder inOrder = inOrder(mStatusLogger, mFailureLogger);

        // Act.
        mPlayCoreLogger.logStatus(moduleName, status);

        // Assert.
        inOrder.verify(mStatusLogger).logStatusChange(moduleName, status);
        inOrder.verify(mFailureLogger).logStatusCanceled(moduleName);
        inOrder.verifyNoMoreInteractions();
    }

    @Test
    public void whenLogDownloadedStatus_verifyLogged() {
        // Arrange.
        String moduleName = "whenLogDownloadedStatus_verifyLogged";
        Integer status = SplitInstallSessionStatus.DOWNLOADED;
        InOrder inOrder = inOrder(mStatusLogger, mFailureLogger);

        // Act.
        mPlayCoreLogger.logStatus(moduleName, status);

        // Assert.
        inOrder.verify(mStatusLogger).logStatusChange(moduleName, status);
        inOrder.verify(mFailureLogger).logStatusNoSplitCompat(moduleName);
        inOrder.verifyNoMoreInteractions();
    }

    @Test
    public void whenLogRequestStart_verifyLogged() {
        // Arrange.
        String moduleName = "whenLogRequestStart_verifyLogged";

        // Act.
        mPlayCoreLogger.logRequestStart(moduleName);

        // Assert.
        verify(mStatusLogger).logRequestStart(moduleName);
        verify(mAvailabilityLogger).storeRequestStart(moduleName);
    }

    @Test
    public void whenLogRequestDeferredStart_verifyLogged() {
        // Arrange.
        String moduleName = "whenLogRequestDeferredStart_verifyLogged";

        // Act.
        mPlayCoreLogger.logRequestDeferredStart(moduleName);

        // Assert.
        verify(mStatusLogger).logRequestDeferredStart(moduleName);
        verify(mAvailabilityLogger).storeRequestDeferredStart(moduleName);
    }

    @Test
    public void whenGetUnknownRequest_verifyCorrectErrorCode() {
        // Arrange.
        Integer expectedCode = SplitInstallFailureLogger.UNKNOWN_REQUEST_ERROR;

        // Act.
        Integer actualCode = mPlayCoreLogger.getUnknownRequestErrorCode();

        // Assert.
        assertEquals(actualCode, expectedCode);
    }
}
