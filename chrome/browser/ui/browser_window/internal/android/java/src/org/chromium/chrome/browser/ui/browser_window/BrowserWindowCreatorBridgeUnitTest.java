// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import static org.junit.Assert.assertEquals;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link BrowserWindowCreatorBridge}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BrowserWindowCreatorBridgeUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Test
    public void createBrowserWindow_returnsNativeBrowserWindowPtr() {
        // Arrange.
        ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowNatives();
        var createParams =
                ChromeAndroidTaskUnitTestSupport.createMockAndroidBrowserWindowCreateParams();
        var tracker = (ChromeAndroidTaskTrackerImpl) ChromeAndroidTaskTrackerFactory.getInstance();

        // Act.
        long result = BrowserWindowCreatorBridge.createBrowserWindow(createParams);

        // Assert.
        var nativeBrowserWindowPtrs = tracker.getAllNativeBrowserWindowPtrs();
        assertEquals(1, nativeBrowserWindowPtrs.length);
        assertEquals(nativeBrowserWindowPtrs[0], result);
    }
}
