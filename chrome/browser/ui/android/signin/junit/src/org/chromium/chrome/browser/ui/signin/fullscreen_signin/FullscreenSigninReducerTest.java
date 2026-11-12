// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.signin.fullscreen_signin;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.signin.fullscreen_signin.FullscreenSigninEffect.RecordNativeInitialized;
import org.chromium.chrome.browser.ui.signin.fullscreen_signin.FullscreenSigninEvent.NativeReady;
import org.chromium.chrome.browser.ui.signin.fullscreen_signin.FullscreenSigninReducer.Result;

import java.util.List;

/** Unit tests for {@link FullscreenSigninReducer}. */
@RunWith(BaseRobolectricTestRunner.class)
@NullMarked
public class FullscreenSigninReducerTest {
    @Test
    public void testNativeReady_recordsNativeInitialized() {
        FullscreenSigninState state = new FullscreenSigninState();
        assertFalse(state.isNativeLoaded());

        Result result = FullscreenSigninReducer.reduce(state, new NativeReady());

        assertTrue(result.getState().isNativeLoaded());
        assertEquals(List.of(RecordNativeInitialized.INSTANCE), result.getEffects());
    }

    @Test
    public void testRepeatedNativeReady_isIgnored() {
        FullscreenSigninState state =
                FullscreenSigninReducer.reduce(new FullscreenSigninState(), new NativeReady())
                        .getState();

        Result result = FullscreenSigninReducer.reduce(state, new NativeReady());

        assertSame(state, result.getState());
        assertTrue(result.getEffects().isEmpty());
    }
}
