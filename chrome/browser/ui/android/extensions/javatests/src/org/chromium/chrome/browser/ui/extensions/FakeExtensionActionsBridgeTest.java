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

import android.graphics.Bitmap;
import android.graphics.Color;
import android.view.KeyEvent;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.extensions.ExtensionActionsBridge.HandleKeyEventResult;
import org.chromium.chrome.browser.ui.extensions.FakeExtensionActionsBridge.ActionData;
import org.chromium.chrome.browser.ui.extensions.FakeExtensionActionsBridge.ProfileModel;
import org.chromium.content_public.browser.WebContents;
import org.chromium.extensions.ShowAction;

@RunWith(BaseRobolectricTestRunner.class)
public class FakeExtensionActionsBridgeTest {
    private static final Bitmap ICON_RED = createSimpleIcon(Color.RED);

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Profile mProfile;
    @Mock private WebContents mWebContents;
    @Mock private ExtensionActionsBridge.Observer mObserver;

    @Rule
    public final FakeExtensionActionsBridgeRule mFakeBridgeRule =
            new FakeExtensionActionsBridgeRule();

    private final FakeExtensionActionsBridge mFakeBridge = mFakeBridgeRule.getFakeBridge();

    private ProfileModel mProfileModel;
    private ExtensionActionsBridge mBridge;

    @Before
    public void setUp() {
        mProfileModel = mFakeBridge.getOrCreateProfileModel(mProfile);
        mBridge = ExtensionActionsBridge.get(mProfile);
        assertEquals(mProfileModel.getBridge(), mBridge);
        mBridge.addObserver(mObserver);
    }

    @Test
    public void testInitialized() {
        assertFalse(mProfileModel.isInitialized());
        assertFalse(mBridge.areActionsInitialized());
        verify(mObserver, never()).onActionModelInitialized();

        mProfileModel.setInitialized(true);

        assertTrue(mProfileModel.isInitialized());
        assertTrue(mBridge.areActionsInitialized());
        verify(mObserver, times(1)).onActionModelInitialized();
    }

    @Test
    public void testKeyEventHandler() {
        KeyEvent eventA = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_A);
        KeyEvent eventB = new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_B);

        assertEquals(new HandleKeyEventResult(false, ""), mBridge.handleKeyDownEvent(eventA));
        assertEquals(new HandleKeyEventResult(false, ""), mBridge.handleKeyDownEvent(eventB));

        mProfileModel.setKeyEventHandler(
                (event) -> {
                    return new HandleKeyEventResult(event.equals(eventA), "");
                });

        assertEquals(new HandleKeyEventResult(true, ""), mBridge.handleKeyDownEvent(eventA));
        assertEquals(new HandleKeyEventResult(false, ""), mBridge.handleKeyDownEvent(eventB));
    }

    @Test
    public void testActionIds() {
        assertArrayEquals(new String[] {}, mBridge.getActionIds());

        mProfileModel.putAction("a", new ActionData.Builder().build());
        mProfileModel.putAction("b", new ActionData.Builder().build());
        mProfileModel.putAction("c", new ActionData.Builder().build());

        assertArrayEquals(new String[] {"a", "b", "c"}, mBridge.getActionIds());
    }

    @Test
    public void testActionMetadata() {
        mProfileModel.putAction("a", new ActionData.Builder().setTitle("foo").build());
        assertEquals(new ExtensionAction("a", "foo"), mBridge.getAction("a", 1));
    }

    @Test
    public void testActionIcon() {
        mProfileModel.putAction("a", new ActionData.Builder().setIcon(ICON_RED).build());
        assertTrue(mBridge.getActionIcon("a", 1, mWebContents, 12, 12, 1.0f).sameAs(ICON_RED));
    }

    @Test
    public void testActionPerTab() {
        mProfileModel.putAction(
                "a", (tabId) -> new ActionData.Builder().setTitle(Integer.toString(tabId)).build());
        assertEquals("1", mBridge.getAction("a", 1).getTitle());
        assertEquals("42", mBridge.getAction("a", 42).getTitle());
    }

    @Test
    public void testActionCallbacks() {
        ActionData data = new ActionData.Builder().build();

        mProfileModel.putAction("a", data);
        verify(mObserver, times(1)).onActionAdded("a");

        mProfileModel.putAction("a", data);
        verify(mObserver, times(1)).onActionUpdated("a");

        mProfileModel.updateActionIcon("a", data);
        verify(mObserver, times(1)).onActionIconUpdated("a");

        mProfileModel.removeAction("a");
        verify(mObserver, times(1)).onActionRemoved("a");
    }

    @Test
    public void testRunAction() {
        mProfileModel.putAction(
                "a", new ActionData.Builder().setActionRunner(() -> ShowAction.NONE).build());
        mProfileModel.putAction(
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
