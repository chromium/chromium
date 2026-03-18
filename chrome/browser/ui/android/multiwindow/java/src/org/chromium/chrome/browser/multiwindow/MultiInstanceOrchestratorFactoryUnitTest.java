// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertThrows;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link MultiInstanceOrchestratorFactory}. */
@RunWith(BaseRobolectricTestRunner.class)
public class MultiInstanceOrchestratorFactoryUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private MultiInstanceOrchestrator mInstance;

    @Before
    public void setup() {
        MultiInstanceOrchestratorFactory.setInstance(null);
    }

    @Test
    public void testGetInstance_throwsExceptionIfNoInstance() {
        assertThrows(IllegalStateException.class, MultiInstanceOrchestratorFactory::getInstance);
    }

    @Test
    public void testGetInstance_returnsRegisteredInstance() {
        MultiInstanceOrchestratorFactory.setInstance(mInstance);
        assertEquals(mInstance, MultiInstanceOrchestratorFactory.getInstance());
    }
}
