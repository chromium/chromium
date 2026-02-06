// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.extensions;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.graphics.Bitmap;
import android.graphics.Color;
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
import org.chromium.chrome.browser.ui.extensions.FakeExtensionActionsBridge.ActionData;
import org.chromium.chrome.browser.ui.extensions.FakeExtensionActionsBridge.TaskModel;
import org.chromium.content_public.browser.WebContents;
import org.chromium.extensions.ShowAction;

@RunWith(BaseRobolectricTestRunner.class)
public class FakeExtensionActionsBridgeTest {
    private static final long BROWSER_WINDOW_POINTER = 1000L;

    private static final Bitmap ICON_RED = createSimpleIcon(Color.RED);

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private ChromeAndroidTask mTask;
    @Mock private WebContents mWebContents;
    @Mock private ExtensionActionsBridge.Observer mObserver;
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
        mBridge.addObserver(mObserver);
    }

    @After
    public void tearDown() {
        mBridge.removeObserver(mObserver);
        mBridge.destroy();
    }

    @Test
    public void testInitialized() {
        assertFalse(mTaskModel.isInitialized());
        assertFalse(mBridge.areActionsInitialized());
        verify(mObserver, never()).onActionModelInitialized();

        mTaskModel.setInitialized(true);

        assertTrue(mTaskModel.isInitialized());
        assertTrue(mBridge.areActionsInitialized());
        verify(mObserver, times(1)).onActionModelInitialized();
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

    @Test
    public void testActionIds() {
        assertArrayEquals(new String[] {}, mBridge.getActionIds());

        mTaskModel.putAction("a", new ActionData.Builder().build());
        mTaskModel.putAction("b", new ActionData.Builder().build());
        mTaskModel.putAction("c", new ActionData.Builder().build());

        assertArrayEquals(new String[] {"a", "b", "c"}, mBridge.getActionIds());
    }

    @Test
    public void testActionMetadata() {
        mTaskModel.putAction("a", new ActionData.Builder().setTitle("foo").build());
        assertEquals(new ExtensionAction("a", "foo"), mBridge.getAction("a", 1));
    }

    @Test
    public void testActionIcon() {
        mTaskModel.putAction("a", new ActionData.Builder().setIcon(ICON_RED).build());
        assertTrue(mBridge.getActionIcon("a", 1, mWebContents, 12, 12, 1.0f).sameAs(ICON_RED));
    }

    @Test
    public void testActionPerTab() {
        mTaskModel.putAction(
                "a", (tabId) -> new ActionData.Builder().setTitle(Integer.toString(tabId)).build());
        assertEquals("1", mBridge.getAction("a", 1).getTitle());
        assertEquals("42", mBridge.getAction("a", 42).getTitle());
    }

    @Test
    public void testActionCallbacks() {
        ActionData data = new ActionData.Builder().build();

        mTaskModel.putAction("a", data);
        verify(mObserver, times(1)).onActionAdded("a");

        mTaskModel.putAction("a", data);
        verify(mObserver, times(1)).onActionUpdated("a");

        mTaskModel.updateActionIcon("a", data);
        verify(mObserver, times(1)).onActionIconUpdated("a");

        mTaskModel.removeAction("a");
        verify(mObserver, times(1)).onActionRemoved("a");
    }

    @Test
    public void testRunAction() {
        mTaskModel.putAction(
                "a", new ActionData.Builder().setActionRunner(() -> ShowAction.NONE).build());
        mTaskModel.putAction(
                "b", new ActionData.Builder().setActionRunner(() -> ShowAction.SHOW_POPUP).build());

        assertEquals(ShowAction.NONE, mBridge.runAction("a", 1, mWebContents));
        assertEquals(ShowAction.SHOW_POPUP, mBridge.runAction("b", 1, mWebContents));
    }

    private static Bitmap createSimpleIcon(int color) {
        Bitmap bitmap = Bitmap.createBitmap(12, 12, Bitmap.Config.ARGB_8888);
        bitmap.eraseColor(color);
        return bitmap;
    }
}
