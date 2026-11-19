// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.embedder_support.delegate;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.Mockito.withSettings;

import android.app.Activity;
import android.app.Dialog;
import android.graphics.Color;

import org.jni_zero.JniRawPtr;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.shadows.ShadowDialog;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;

/** Unit tests for {@link HtmlColorPickerBridge}. */
@RunWith(BaseRobolectricTestRunner.class)
public class HtmlColorPickerBridgeUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private HtmlColorPickerBridge.Natives mNatives;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private WindowAndroid mNewWindowAndroid;
    @Mock private Callback<Integer> mDialogDismissedCallback;

    private Activity mActivity;
    private WebContents mWebContents;
    private WebContentsObserver mWebContentsObserver;
    private JniRawPtr<HtmlColorPickerBridge.NativeHtmlColorPickerBridge> mNativeColorPicker;

    @Before
    public void setUp() {
        HtmlColorPickerBridgeJni.setInstanceForTesting(mNatives);
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        when(mWindowAndroid.getContext()).thenReturn(new WeakReference<>(mActivity));
        mWebContents =
                mock(
                        WebContents.class,
                        withSettings().extraInterfaces(WebContentsObserver.Observable.class));
        doAnswer(
                        invocation -> {
                            mWebContentsObserver = invocation.getArgument(0);
                            return null;
                        })
                .when((WebContentsObserver.Observable) mWebContents)
                .addObserver(any());
    }

    @After
    public void tearDown() {
        // Native releases this when it destroys the bridge, which the mocked natives don't do.
        if (mNativeColorPicker != null) mNativeColorPicker.release();
    }

    private Dialog showColorPicker(int initialColor) {
        mNativeColorPicker = JniRawPtr.createForTesting(1L);
        HtmlColorPickerBridge bridge =
                HtmlColorPickerBridge.create(mNativeColorPicker, mWindowAndroid, mWebContents);
        assertNotNull(bridge);
        bridge.showColorPicker(initialColor);
        Dialog dialog = ShadowDialog.getLatestDialog();
        assertTrue(dialog.isShowing());
        return dialog;
    }

    @Test
    public void testCancelsWhenWebContentsLeavesWindow() {
        Dialog dialog = showColorPicker(Color.RED);

        // The WebContents moves to another window, e.g. for a new Activity.
        mWebContentsObserver.onTopLevelNativeWindowChanged(mNewWindowAndroid);

        assertFalse(dialog.isShowing());
        verify(mNatives).onColorChosen(any(), eq(Color.RED));
    }

    @Test
    public void testStaysOpenWhileWebContentsKeepsWindow() {
        Dialog dialog = showColorPicker(Color.RED);

        mWebContentsObserver.onTopLevelNativeWindowChanged(mWindowAndroid);

        assertTrue(dialog.isShowing());
        verify(mNatives, never()).onColorChosen(any(), anyInt());
    }

    @Test
    public void testCancelWhenDialogWindowIsGone() {
        // Destroying an Activity removes its dialogs' windows without dismissing them, so a later
        // dismiss() throws.
        HtmlColorPickerDialogView dialogView = spy(new HtmlColorPickerDialogView(mActivity));
        doThrow(new IllegalArgumentException("View not attached to window manager"))
                .when(dialogView)
                .dismiss();
        HtmlColorPickerCoordinator coordinator =
                new HtmlColorPickerCoordinator(mActivity, mDialogDismissedCallback, dialogView);
        coordinator.show(Color.RED);

        coordinator.cancel();

        verify(mDialogDismissedCallback).onResult(Color.RED);
    }
}
