// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.security;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import android.view.MotionEvent;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link SecurityTouchEventInterceptionHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SecurityTouchEventInterceptionHelperUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private MotionEvent mMotionEvent;

    private SecurityTouchEventInterceptionHelper mHelper;

    @Before
    public void setUp() {
        mHelper = new SecurityTouchEventInterceptionHelper();
    }

    @Test
    public void testIsWindowObscured_PartiallyObscured() {
        when(mMotionEvent.getFlags()).thenReturn(MotionEvent.FLAG_WINDOW_IS_PARTIALLY_OBSCURED);
        mHelper.registerMotionEvent(mMotionEvent);
        assertTrue(mHelper.isWindowObscured());
    }

    @Test
    public void testIsWindowObscured_Obscured() {
        when(mMotionEvent.getFlags()).thenReturn(MotionEvent.FLAG_WINDOW_IS_OBSCURED);
        mHelper.registerMotionEvent(mMotionEvent);
        assertTrue(mHelper.isWindowObscured());
    }

    @Test
    public void testIsWindowObscured_PartiallyObscuredAndObscured() {
        when(mMotionEvent.getFlags())
                .thenReturn(
                        MotionEvent.FLAG_WINDOW_IS_PARTIALLY_OBSCURED
                                | MotionEvent.FLAG_WINDOW_IS_OBSCURED);
        mHelper.registerMotionEvent(mMotionEvent);
        assertTrue(mHelper.isWindowObscured());
    }

    @Test
    public void testIsWindowObscured_NotObscured() {
        when(mMotionEvent.getFlags()).thenReturn(0);
        mHelper.registerMotionEvent(mMotionEvent);
        assertFalse(mHelper.isWindowObscured());
    }

    @Test
    public void testIsWindowObscured_MultipleEvents() {
        when(mMotionEvent.getFlags()).thenReturn(0);
        mHelper.registerMotionEvent(mMotionEvent);
        assertFalse(mHelper.isWindowObscured());

        when(mMotionEvent.getFlags()).thenReturn(MotionEvent.FLAG_WINDOW_IS_PARTIALLY_OBSCURED);
        mHelper.registerMotionEvent(mMotionEvent);
        assertTrue(mHelper.isWindowObscured());

        when(mMotionEvent.getFlags()).thenReturn(0);
        mHelper.registerMotionEvent(mMotionEvent);
        assertFalse(mHelper.isWindowObscured());
    }
}
