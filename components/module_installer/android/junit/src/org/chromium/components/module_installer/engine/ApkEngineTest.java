// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.module_installer.engine;

import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Test suite for the ApkEngine class. */
@RunWith(BaseRobolectricTestRunner.class)
public class ApkEngineTest {
    private ApkEngine mEngine;

    @Before
    public void setUp() {
        mEngine = new ApkEngine();
    }

    @Test
    public void whenInstalling_verifyCompleteWithFailure() {
        // Arrange.
        String moduleName = "whenInstalling_verifyCompleteWithFailure";
        InstallListener listener = mock(InstallListener.class);
        InOrder inOrder = inOrder(listener);

        // Act.
        mEngine.install(moduleName, listener);

        // Assert.
        inOrder.verify(listener).onComplete(false);
        inOrder.verifyNoMoreInteractions();
    }
}
