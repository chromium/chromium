// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.module_installer.builder;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.module_installer.engine.InstallEngine;
import org.chromium.components.module_installer.engine.InstallListener;

/** Test suite for the Module class. */
@RunWith(BaseRobolectricTestRunner.class)
public class ModuleTest {
    @Mock private InstallEngine mInstallEngineMock;

    private final String mModuleName = "module_stub";
    private final Class mInterface = ModuleTestStubInterface.class;
    private final String mImplName = ModuleTestStub.class.getName();

    private Module<ModuleTestStub> mModule;

    /**
     * This class needs to be static (for testing purposes).
     * https://stackoverflow.com/questions/31396758/why-i-get-java-lang-instantiationexception-here
     */
    public static class ModuleTestStub implements ModuleTestStubInterface {}

    private interface ModuleTestStubInterface {}

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mModule = new Module<ModuleTestStub>(mModuleName, mInterface, mImplName);
        mModule.setInstallEngine(mInstallEngineMock);
    }

    @Test
    public void whenVerifyingIsInstalled_VerifySequence() {
        // Arrange.
        InOrder inOrder = inOrder(mInstallEngineMock);

        // Act.
        mModule.isInstalled();

        // Assert.
        inOrder.verify(mInstallEngineMock).isInstalled(mModuleName);
        inOrder.verifyNoMoreInteractions();
    }

    @Test
    public void whenInstalling_VerifySequence() {
        // Arrange.
        InstallListener listenerMock = mock(InstallListener.class);
        InOrder inOrder = inOrder(mInstallEngineMock, listenerMock);

        // Act.
        mModule.install(listenerMock);

        // Assert.
        inOrder.verify(mInstallEngineMock).install(mModuleName, listenerMock);
        inOrder.verifyNoMoreInteractions();
    }

    @Test
    public void whenInstallingDeferred_VerifySequence() {
        // Arrange.
        InOrder inOrder = inOrder(mInstallEngineMock);

        // Act.
        mModule.installDeferred();

        // Assert.
        inOrder.verify(mInstallEngineMock).installDeferred(mModuleName);
        inOrder.verifyNoMoreInteractions();
    }

    @Test
    public void whenGetImpl_VerifyCorrectInstance() {
        // Arrange.
        Class expectedType = ModuleTestStub.class;
        doReturn(true).when(mInstallEngineMock).isInstalled(mModuleName);

        // Act.
        ModuleTestStub impl = mModule.getImpl();

        // Assert.
        assertEquals(expectedType, impl.getClass());
    }

    @Test(expected = RuntimeException.class)
    public void whenGettingUnknownImpl_VerifyError() {
        // Arrange.
        String impl = "some unknown type";
        Module<ModuleTestStub> module = new Module<ModuleTestStub>(mModuleName, mInterface, impl);
        doReturn(true).when(mInstallEngineMock).isInstalled(mModuleName);
        module.setInstallEngine(mInstallEngineMock);

        // Act & Assert.
        module.getImpl();
    }
}
