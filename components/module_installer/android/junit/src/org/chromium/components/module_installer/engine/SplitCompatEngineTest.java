// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.module_installer.engine;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;

import com.google.android.play.core.splitinstall.SplitInstallException;
import com.google.android.play.core.splitinstall.SplitInstallManager;
import com.google.android.play.core.splitinstall.SplitInstallRequest;
import com.google.android.play.core.splitinstall.SplitInstallSessionState;
import com.google.android.play.core.splitinstall.SplitInstallStateUpdatedListener;
import com.google.android.play.core.splitinstall.model.SplitInstallErrorCode;
import com.google.android.play.core.splitinstall.model.SplitInstallSessionStatus;
import com.google.android.play.core.tasks.OnFailureListener;
import com.google.android.play.core.tasks.Task;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.module_installer.logger.Logger;

import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Test suite for the SplitCompatEngine class.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class SplitCompatEngineTest {
    @Mock
    private Logger mLogger;
    @Mock
    private SplitInstallManager mManager;
    @Mock
    private SplitInstallRequest mInstallRequest;
    @Mock
    private Task<Integer> mTask;

    private SplitCompatEngine mInstaller;
    private SplitCompatEngineFacade mInstallerFacade;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mInstallerFacade = mock(SplitCompatEngineFacade.class);

        // Mock SplitCompatEngineFacade.
        doReturn(mLogger).when(mInstallerFacade).getLogger();
        doReturn(mManager).when(mInstallerFacade).getSplitManager();
        doReturn(mInstallRequest).when(mInstallerFacade).createSplitInstallRequest(any());

        // Mock SplitInstallManager.
        doReturn(mTask).when(mManager).startInstall(any());

        mInstaller = new SplitCompatEngine(mInstallerFacade);

        mInstaller.resetSessionQueue();
    }

    @Test
    public void whenConstructed_verifySplitInitialized() {
        // Arrange.
        InOrder inOrder = inOrder(mInstallerFacade, mManager);

        // Act & Assert.
        inOrder.verify(mInstallerFacade).initApplicationContext(mInstaller);
        inOrder.verifyNoMoreInteractions();
    }

    @Test
    public void whenInitActivity_verifyActivityInstalled() {
        // Arrange.
        Activity activityMock = mock(Activity.class);

        // Act.
        mInstaller.initActivity(activityMock);

        // Assert.
        verify(mInstallerFacade, times(1)).installActivity(activityMock);
    }

    @Test
    public void whenIsInstalled_verifyModuleIsInstalled() {
        // Arrange.
        String installedModule = "m1";
        String uninstalledModule = "m2";
        Set<String> installedModules = new HashSet<String>() {
            { add(installedModule); }
        };
        doReturn(installedModules).when(mManager).getInstalledModules();

        // Act & Assert.
        assertTrue(mInstaller.isInstalled(installedModule));
        assertFalse(mInstaller.isInstalled(uninstalledModule));
    }

    @Test
    public void whenInstallDeferred_verifyModuleInstalled() {
        // Arrange.
        String moduleName = "whenInstallDeferred_verifyModuleInstalled";
        List<String> moduleList = Collections.singletonList(moduleName);

        // Act.
        mInstaller.installDeferred(moduleName);

        // Assert.
        verify(mManager, times(1)).deferredInstall(moduleList);
        verify(mLogger, times(1)).logRequestDeferredStart(moduleName);
    }

    @Test
    public void whenInstalling_verifyInstallSequence() {
        // Arrange.
        String moduleName = "whenInstalling_verifyInstallSequence";
        InstallListener listener = mock(InstallListener.class);
        InOrder inOrder = inOrder(mInstallerFacade, mManager, mLogger, mTask);

        // Act.
        mInstaller.install(moduleName, listener);

        // Assert.
        inOrder.verify(mManager).registerListener(any());
        inOrder.verify(mInstallerFacade).createSplitInstallRequest(moduleName);
        inOrder.verify(mManager).startInstall(mInstallRequest);
        inOrder.verify(mTask).addOnFailureListener(any());
        inOrder.verify(mLogger).logRequestStart(moduleName);
        inOrder.verifyNoMoreInteractions();
    }

    @Test
    public void whenInstallingSameModuleConcurrently_verifySingleInstall() {
        // Arrange.
        String moduleName = "whenInstallingSameModuleConcurrently_verifySingleInstall";
        InstallListener listener = mock(InstallListener.class);
        SplitCompatEngine instance1 = new SplitCompatEngine(mInstallerFacade);
        SplitCompatEngine instance2 = new SplitCompatEngine(mInstallerFacade);

        // Act.
        instance1.install(moduleName, listener);
        instance1.install(moduleName, listener);
        instance2.install(moduleName, listener);
        instance2.install(moduleName, listener);

        // Assert.
        verify(mInstallerFacade, times(1)).createSplitInstallRequest(moduleName);
    }

    @Test
    public void whenInstallingWithException_verifyErrorHandled() {
        // Arrange.
        String moduleName = "whenInstallingWithException_verifyErrorHandled";
        String exceptionMessage = moduleName + "_ex_msg";
        Integer errorCode = -1;
        InstallListener listener = mock(InstallListener.class);
        ArgumentCaptor<OnFailureListener> arg = ArgumentCaptor.forClass(OnFailureListener.class);
        doReturn(errorCode).when(mLogger).getUnknownRequestErrorCode();

        // Act.
        mInstaller.install(moduleName, listener);
        verify(mTask).addOnFailureListener(arg.capture());
        arg.getValue().onFailure(new Exception(exceptionMessage));

        // Assert.
        verify(mLogger, times(1)).logRequestFailure(moduleName, errorCode);
        verify(listener, times(1)).onComplete(false);
    }

    @Test
    public void whenInstallingWithSplitException_verifyErrorHandled() {
        // Arrange.
        String moduleName = "whenInstallingWithSplitException_verifyErrorHandled";
        InstallListener listener = mock(InstallListener.class);
        ArgumentCaptor<OnFailureListener> arg = ArgumentCaptor.forClass(OnFailureListener.class);

        // Act.
        mInstaller.install(moduleName, listener);
        verify(mTask).addOnFailureListener(arg.capture());
        arg.getValue().onFailure(new SplitInstallException(-1));

        // Assert.
        verify(mLogger, times(1)).logRequestFailure(moduleName, -1);
        verify(listener, times(1)).onComplete(false);
    }

    @Test
    public void whenInstallingWithException_verifyCanTryAgainAfterFailure() {
        // Arrange.
        String moduleName = "whenInstallingWithException_verifyCanTryAgainAfterFailure";
        ArgumentCaptor<OnFailureListener> arg = ArgumentCaptor.forClass(OnFailureListener.class);

        // Act.
        mInstaller.install(moduleName, mock(InstallListener.class));
        verify(mTask).addOnFailureListener(arg.capture());
        arg.getValue().onFailure(new Exception(""));
        mInstaller.install(moduleName, mock(InstallListener.class)); // 2nd call.

        // Assert.
        verify(mLogger, times(2)).logRequestStart(moduleName);
    }

    @Test(expected = UnsupportedOperationException.class)
    public void whenInstallingWithMoreThanOneModule_verifyException() {
        // Arrange.
        String moduleName = "whenInstallingWithMoreThanOneModule_verifyException";
        InstallListener listener = mock(InstallListener.class);

        // Mock SplitInstallSessionState.
        SplitInstallSessionState state = mock(SplitInstallSessionState.class);
        doReturn(Arrays.asList("m1", "m2")).when(state).moduleNames();

        ArgumentCaptor<SplitInstallStateUpdatedListener> arg =
                ArgumentCaptor.forClass(SplitInstallStateUpdatedListener.class);

        // Act & Assert.
        mInstaller.install(moduleName, listener);
        verify(mManager).registerListener(arg.capture());
        arg.getValue().onStateUpdate(state);
    }

    @Test
    public void whenInstalled_verifyListenerAndLogger() {
        // Arrange.
        String moduleName = "whenInstalled_verifyListenerAndLogger";
        Integer status = SplitInstallSessionStatus.INSTALLED;
        InstallListener listener = mock(InstallListener.class);

        // Mock SplitInstallSessionState.
        SplitInstallSessionState state = mock(SplitInstallSessionState.class);
        doReturn(status).when(state).status();
        doReturn(Arrays.asList(moduleName)).when(state).moduleNames();

        InOrder inOrder = inOrder(listener, mManager, mLogger);
        ArgumentCaptor<SplitInstallStateUpdatedListener> arg =
                ArgumentCaptor.forClass(SplitInstallStateUpdatedListener.class);

        // Act.
        mInstaller.install(moduleName, listener);
        verify(mManager).registerListener(arg.capture());
        arg.getValue().onStateUpdate(state);

        // Assert.
        inOrder.verify(listener, times(1)).onComplete(true);
        inOrder.verify(mManager, times(1)).unregisterListener(any());
        inOrder.verify(mLogger, times(1)).logStatus(moduleName, status);
        inOrder.verifyNoMoreInteractions();
    }

    @Test
    public void whenFailureToInstall_verifyListenerAndLogger() {
        // Arrange.
        String moduleName = "whenFailureToInstall_verifyListenerAndLogger";
        Integer status = SplitInstallSessionStatus.FAILED;
        Integer errorCode = SplitInstallErrorCode.NO_ERROR;
        InstallListener listener = mock(InstallListener.class);

        // Mock SplitInstallSessionState.
        SplitInstallSessionState state = mock(SplitInstallSessionState.class);
        doReturn(status).when(state).status();
        doReturn(errorCode).when(state).errorCode();
        doReturn(Arrays.asList(moduleName)).when(state).moduleNames();

        InOrder inOrder = inOrder(listener, mLogger, mManager);
        ArgumentCaptor<SplitInstallStateUpdatedListener> arg =
                ArgumentCaptor.forClass(SplitInstallStateUpdatedListener.class);

        // Act.
        mInstaller.install(moduleName, listener);
        verify(mManager).registerListener(arg.capture());
        arg.getValue().onStateUpdate(state);

        // Assert.
        inOrder.verify(listener, times(1)).onComplete(false);
        inOrder.verify(mManager, times(1)).unregisterListener(any());
        inOrder.verify(mLogger, times(1)).logStatusFailure(moduleName, errorCode);
        inOrder.verify(mLogger, times(1)).logStatus(moduleName, status);
        inOrder.verifyNoMoreInteractions();
    }

    @Test
    public void whenNotInstalledOrFailed_verifyStatusLogged() {
        // Arrange.
        String moduleName = "whenNotInstalledOrFailed_verifyStatusLogged";
        Integer status = SplitInstallSessionStatus.UNKNOWN;
        InstallListener listener = mock(InstallListener.class);

        // Mock SplitInstallSessionState.
        SplitInstallSessionState state = mock(SplitInstallSessionState.class);
        doReturn(status).when(state).status();
        doReturn(Arrays.asList(moduleName)).when(state).moduleNames();

        InOrder inOrder = inOrder(listener, mLogger);
        ArgumentCaptor<SplitInstallStateUpdatedListener> arg =
                ArgumentCaptor.forClass(SplitInstallStateUpdatedListener.class);

        // Act.
        mInstaller.install(moduleName, mock(InstallListener.class));
        verify(mManager).registerListener(arg.capture());
        arg.getValue().onStateUpdate(state);

        // Assert.
        inOrder.verify(mLogger, times(1)).logStatus(moduleName, status);
        inOrder.verifyNoMoreInteractions();
    }
}
