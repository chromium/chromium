// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.module_installer.builder;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;

import android.app.Activity;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.module_installer.engine.EngineFactory;
import org.chromium.components.module_installer.engine.InstallEngine;
import org.chromium.components.module_installer.engine.InstallListener;

/** Test suite for the ModuleEngine class. */
@RunWith(BaseRobolectricTestRunner.class)
public class ModuleEngineTest {
    @Mock private InstallEngine mInstallEngineMock;

    @Mock private EngineFactory mEngineFactoryMock;

    private ModuleEngine mModuleEngine;

    private static class ModuleStub {}

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        doReturn(mInstallEngineMock).when(mEngineFactoryMock).getEngine();

        mModuleEngine = new ModuleEngine(ModuleStub.class.getName(), mEngineFactoryMock);
    }

    @Test
    public void whenInitActivity_verifyActivityInitialized() {
        // Arrange.
        Activity activityMock = mock(Activity.class);
        InOrder inOrder = inOrder(mInstallEngineMock);

        // Act.
        mModuleEngine.initActivity(activityMock);

        // Assert.
        inOrder.verify(mInstallEngineMock).initActivity(activityMock);
        inOrder.verifyNoMoreInteractions();
    }

    @Test
    public void whenVerifyingIsInstalled_VerifyTrue() {
        // Arrange.
        String moduleName = "any name";

        // Act.
        Boolean isInstalled = mModuleEngine.isInstalled(moduleName);

        // Assert.
        assertTrue(isInstalled);
    }

    @Test
    public void whenVerifyingIsInstalled_VerifyFalse() {
        // Arrange.
        String moduleName = "any name";
        ModuleEngine engine = new ModuleEngine("non_existent_class", mEngineFactoryMock);

        // Act.
        Boolean isInstalled = engine.isInstalled(moduleName);

        // Assert.
        assertFalse(isInstalled);
    }

    @Test
    public void whenInstallDeferred_verifyInstalled() {
        // Arrange.
        String moduleName = "whenInstallDeferred_verifyInstalled";
        InOrder inOrder = inOrder(mInstallEngineMock);

        // Act.
        mModuleEngine.installDeferred(moduleName);

        // Assert.
        inOrder.verify(mInstallEngineMock).installDeferred(moduleName);
        inOrder.verifyNoMoreInteractions();
    }

    @Test
    public void whenInstall_verifyInstalled() {
        // Arrange.
        String moduleName = "whenInstall_verifyInstalled";
        InstallListener listenerMock = mock(InstallListener.class);
        InOrder inOrder = inOrder(mInstallEngineMock);

        // Act.
        mModuleEngine.install(moduleName, listenerMock);

        // Assert.
        inOrder.verify(mInstallEngineMock).install(moduleName, listenerMock);
        inOrder.verifyNoMoreInteractions();
    }
}
