// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.mock;

import android.graphics.Rect;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.mojom.WindowShowState;

/** Unit tests for {@link AndroidBrowserWindowCreateParamsImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AndroidBrowserWindowCreateParamsImplUnitTest {

    @Test
    public void testCreate() {
        // Arrange.
        Profile profile = mock(Profile.class);

        @BrowserWindowType int windowType = BrowserWindowType.NORMAL;
        int leftBound = 10;
        int topBound = 20;
        int width = 300;
        int height = 400;
        int initialShowState = WindowShowState.NORMAL;

        // Act.
        AndroidBrowserWindowCreateParams params =
                AndroidBrowserWindowCreateParamsImpl.create(
                        windowType, profile, leftBound, topBound, width, height, initialShowState);

        // Assert.
        assertEquals("Window type should match.", windowType, params.getWindowType());
        assertEquals("Profile should match.", profile, params.getProfile());
        assertEquals(
                "Initial bounds should match.",
                new Rect(leftBound, topBound, width, height),
                params.getInitialBounds());
        assertEquals(
                "Initial show state should match.", initialShowState, params.getInitialShowState());
    }
}
