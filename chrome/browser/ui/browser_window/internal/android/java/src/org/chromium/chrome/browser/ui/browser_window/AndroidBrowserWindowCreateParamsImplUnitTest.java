// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.verify;

import android.graphics.Rect;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.mojom.WindowShowState;

/** Unit tests for {@link AndroidBrowserWindowCreateParamsImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AndroidBrowserWindowCreateParamsImplUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Profile mProfile;
    @Mock private WebContents mWebContents;

    @Test
    public void testCreate() {
        // Arrange.

        @BrowserWindowType int windowType = BrowserWindowType.NORMAL;
        int leftBound = 10;
        int topBound = 20;
        int width = 300;
        int height = 400;
        int initialShowState = WindowShowState.NORMAL;

        // Act.
        AndroidBrowserWindowCreateParams params =
                AndroidBrowserWindowCreateParamsImpl.create(
                        windowType,
                        mProfile,
                        leftBound,
                        topBound,
                        width,
                        height,
                        initialShowState,
                        mWebContents);

        // Assert.
        assertEquals("Window type should match.", windowType, params.getWindowType());
        assertEquals("Profile should match.", mProfile, params.getProfile());
        assertEquals(
                "Initial bounds should match.",
                new Rect(leftBound, topBound, width, height),
                params.getInitialBoundsInDp());
        assertEquals(
                "Initial show state should match.", initialShowState, params.getInitialShowState());
        assertEquals("WebContents should match.", mWebContents, params.takeWebContents());
    }

    @Test
    public void testTakeWebContents() {
        // Arrange.

        AndroidBrowserWindowCreateParams params =
                AndroidBrowserWindowCreateParamsImpl.create(
                        BrowserWindowType.NORMAL,
                        mProfile,
                        0,
                        0,
                        100,
                        100,
                        WindowShowState.NORMAL,
                        mWebContents);

        // Act & Assert.
        assertEquals(
                "takeWebContents should return WebContents.",
                mWebContents,
                params.takeWebContents());
        assertNull("Subsequent takeWebContents should return null.", params.takeWebContents());
    }

    @Test
    public void testDestroyWebContents() {
        // Arrange.

        AndroidBrowserWindowCreateParams params =
                AndroidBrowserWindowCreateParamsImpl.create(
                        BrowserWindowType.NORMAL,
                        mProfile,
                        0,
                        0,
                        100,
                        100,
                        WindowShowState.NORMAL,
                        mWebContents);

        // Act.
        params.destroyWebContents();

        // Assert.
        verify(mWebContents).destroy();
        assertNull(
                "WebContents should be null after destroyWebContents.", params.takeWebContents());
    }
}
