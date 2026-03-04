// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.extensions;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.when;

import android.view.KeyEvent;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.chrome.browser.ui.extensions.ExtensionActionsBridge.HandleKeyEventResult;
import org.chromium.chrome.browser.ui.extensions.FakeExtensionActionsBridge.TaskModel;

@RunWith(BaseRobolectricTestRunner.class)
public class FakeExtensionActionsBridgeTest {
    private static final long BROWSER_WINDOW_POINTER = 1000L;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private ChromeAndroidTask mTask;
    @Mock private Profile mProfile;

    @Rule
    public final FakeExtensionActionsBridgeRule mFakeBridgeRule =
            new FakeExtensionActionsBridgeRule();

    private final FakeExtensionActionsBridge mFakeBridge = mFakeBridgeRule.getFakeBridge();

    private TaskModel mTaskModel;
    private ExtensionActionsBridge mBridge;

    @Before
    public void setUp() {
        when(mTask.getOrCreateNativeBrowserWindowPtr(mProfile)).thenReturn(BROWSER_WINDOW_POINTER);
        mTaskModel = mFakeBridge.getOrCreateTaskModel(mTask, mProfile);
        mBridge = new ExtensionActionsBridge(mTask, mProfile);
    }

    @After
    public void tearDown() {
        mBridge.destroy();
    }

    @Test
    public void testKeyEventHandler() {
        KeyEvent eventA = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_A);
        KeyEvent eventB = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_B);

        assertEquals(new HandleKeyEventResult(false, ""), mBridge.handleKeyDownEvent(eventA));
        assertEquals(new HandleKeyEventResult(false, ""), mBridge.handleKeyDownEvent(eventB));

        mTaskModel.setKeyEventHandler(
                (event) -> {
                    return new HandleKeyEventResult(event.equals(eventA), "");
                });

        assertEquals(new HandleKeyEventResult(true, ""), mBridge.handleKeyDownEvent(eventA));
        assertEquals(new HandleKeyEventResult(false, ""), mBridge.handleKeyDownEvent(eventB));
    }
}
