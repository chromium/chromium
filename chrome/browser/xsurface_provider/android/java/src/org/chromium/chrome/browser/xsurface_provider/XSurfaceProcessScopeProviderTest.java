// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xsurface_provider;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ServiceLoaderUtil;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.xsurface.ProcessScope;
import org.chromium.chrome.browser.xsurface_provider.hooks.XSurfaceHooks;

/** Tests for {@link XSurfaceProcessScopeProvider}. */
@RunWith(BaseRobolectricTestRunner.class)
public class XSurfaceProcessScopeProviderTest {
    @Mock private XSurfaceHooks mXSurfaceHooks;
    @Mock private ProcessScope mProcessScope;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        ServiceLoaderUtil.setInstanceForTesting(XSurfaceHooks.class, mXSurfaceHooks);
    }

    @Test
    public void getProcessScope_hooksDisabled_returnsNull() {
        when(mXSurfaceHooks.isEnabled()).thenReturn(false);
        assertNull(null, XSurfaceProcessScopeProvider.getProcessScope());
    }

    @Test
    public void getProcessScope_hooksEnabled_returnsProcessScopeFromHooks() {
        when(mXSurfaceHooks.isEnabled()).thenReturn(true);
        when(mXSurfaceHooks.createProcessScope(any())).thenReturn(mProcessScope);

        assertEquals(mProcessScope, XSurfaceProcessScopeProvider.getProcessScope());
    }

    @Test
    public void getProcessScope_hooksEnabled_reusesProcessScopeWhenRequestedAgain() {
        when(mXSurfaceHooks.isEnabled()).thenReturn(true);
        when(mXSurfaceHooks.createProcessScope(any())).thenReturn(mProcessScope);

        ProcessScope processScope = XSurfaceProcessScopeProvider.getProcessScope();

        when(mXSurfaceHooks.isEnabled()).thenReturn(false);
        when(mXSurfaceHooks.createProcessScope(any())).thenReturn(null);

        assertEquals(processScope, XSurfaceProcessScopeProvider.getProcessScope());
    }
}
