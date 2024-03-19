// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.stylus_handwriting;

import static android.view.PointerIcon.TYPE_HANDWRITING;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.openMocks;

import android.content.ComponentName;
import android.content.Context;
import android.graphics.Point;
import android.graphics.Rect;
import android.graphics.RectF;
import android.os.Build;
import android.view.ViewGroup;
import android.view.inputmethod.EditorBoundsInfo;
import android.view.inputmethod.InputMethodInfo;
import android.view.inputmethod.InputMethodManager;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.components.stylus_handwriting.test_support.ShadowGlobalSettings;
import org.chromium.components.stylus_handwriting.test_support.ShadowSecureSettings;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ViewAndroidDelegate;

import java.util.List;

/** Unit tests for {@link AndroidStylusWritingHandler}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        sdk = Build.VERSION_CODES.TIRAMISU,
        shadows = {ShadowGlobalSettings.class, ShadowSecureSettings.class})
public class AndroidStylusWritingHandlerTest {
    @Mock private Context mContext;
    @Mock private InputMethodManager mInputMethodManager;
    @Mock private InputMethodInfo mInputMethodInfo;

    private AndroidStylusWritingHandler mHandler;

    @Before
    public void setUp() {
        openMocks(this);
        when(mContext.getSystemService(InputMethodManager.class)).thenReturn(mInputMethodManager);
        when(mInputMethodManager.getInputMethodList()).thenReturn(List.of(mInputMethodInfo));
        when(mInputMethodInfo.getComponent())
                .thenReturn(
                        ComponentName.unflattenFromString(
                                ShadowSecureSettings.DEFAULT_IME_PACKAGE));
        mHandler = new AndroidStylusWritingHandler(mContext);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.S)
    public void handlerIsDisabled_beforeT() {
        assertFalse(AndroidStylusWritingHandler.isEnabled(mContext));
    }

    @Test
    public void handlerIsDisabled_withFeatureDisabled() {
        when(mInputMethodInfo.supportsStylusHandwriting()).thenReturn(true);
        ShadowGlobalSettings.setHandwritingEnabled(false);
        assertFalse(AndroidStylusWritingHandler.isEnabled(mContext));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void handlerUsesSecureSettings_onAndroidU() {
        when(mInputMethodInfo.supportsStylusHandwriting()).thenReturn(true);
        ShadowGlobalSettings.setHandwritingEnabled(false);
        ShadowSecureSettings.setHandwritingEnabled(true);
        assertTrue(AndroidStylusWritingHandler.isEnabled(mContext));
    }

    @Test
    public void handlerIsDisabled_noKeyboard() {
        ShadowGlobalSettings.setHandwritingEnabled(true);
        when(mInputMethodManager.getInputMethodList()).thenReturn(List.of());
        assertFalse(AndroidStylusWritingHandler.isEnabled(mContext));
    }

    @Test
    public void handlerIsDisabled_noKeyboardSupport() {
        when(mInputMethodInfo.supportsStylusHandwriting()).thenReturn(false);
        ShadowGlobalSettings.setHandwritingEnabled(true);
        assertFalse(AndroidStylusWritingHandler.isEnabled(mContext));
    }

    @Test
    public void handlerIsEnabled_withFeatureEnabledAndKeyboardSupport() {
        when(mInputMethodInfo.supportsStylusHandwriting()).thenReturn(true);
        ShadowGlobalSettings.setHandwritingEnabled(true);
        assertTrue(AndroidStylusWritingHandler.isEnabled(mContext));
    }

    @Test
    public void webContentsChanged_StylusWritingHandlerIsSet() {
        WebContents webContents = mock(WebContents.class);
        ViewGroup containerView = mock(ViewGroup.class);
        ViewAndroidDelegate viewAndroidDelegate =
                ViewAndroidDelegate.createBasicDelegate(containerView);
        when(webContents.getViewAndroidDelegate()).thenReturn(viewAndroidDelegate);

        mHandler.onWebContentsChanged(mContext, webContents);
        verify(webContents).setStylusWritingHandler(eq(mHandler));
    }

    @Test
    public void testAutoHandwritingIsDisabled() {
        WebContents webContents = mock(WebContents.class);
        ViewGroup containerView = mock(ViewGroup.class);
        ViewAndroidDelegate viewAndroidDelegate =
                ViewAndroidDelegate.createBasicDelegate(containerView);
        when(webContents.getViewAndroidDelegate()).thenReturn(viewAndroidDelegate);

        mHandler.onWebContentsChanged(mContext, webContents);
        // Ensure we notify Android that we handle our own handwriting.
        verify(containerView).setAutoHandwritingEnabled(false);
    }

    @Test
    public void testShowSoftKeyboardEnabled() {
        assertTrue(mHandler.canShowSoftKeyboard());
    }

    @Test
    public void editElementFocusedReturnsDips() {
        Rect boundsInPix = new Rect(20, 20, 80, 80);
        Point cursorPositionInPix = new Point(40, 40);
        // onEditElementFocusedForStylusWriting receives coordinates in pixels and should convert
        // them to DIPs. It does not use the contentOffset as this is taken into account by
        // CursorAnchorInfo's matrix.
        EditorBoundsInfo editorBoundsInfo =
                mHandler.onEditElementFocusedForStylusWriting(
                        boundsInPix, cursorPositionInPix, 4, 10, null);
        assertEquals(new RectF(5, 5, 20, 20), editorBoundsInfo.getEditorBounds());
        assertEquals(new RectF(5, 5, 20, 20), editorBoundsInfo.getHandwritingBounds());
    }

    @Test
    public void onFocusedNodeChangedReturnsDips() {
        Rect boundsInDips = new Rect(20, 20, 80, 80);
        // onFocusedNodeChanged receives coordinates in DIPs and should return them in the same
        // format. It does not use the contentOffset as this is taken into account by
        // CursorAnchorInfo's matrix.
        EditorBoundsInfo editorBoundsInfo =
                mHandler.onFocusedNodeChanged(boundsInDips, true, null, 4, 10);
        assertEquals(new RectF(boundsInDips), editorBoundsInfo.getEditorBounds());
        assertEquals(new RectF(boundsInDips), editorBoundsInfo.getHandwritingBounds());
    }

    /**
     * Verify that stylus handwriting is started after edit element is focused so that
     * InputConnection is current focused element. See https://crbug.com/1512519
     */
    @Test
    public void testStartStylusHandwriting() {
        ViewGroup containerView = mock(ViewGroup.class);
        Rect boundsInPix = new Rect(20, 20, 80, 80);
        Point cursorPositionInPix = new Point(40, 40);
        mHandler.onEditElementFocusedForStylusWriting(
                boundsInPix, cursorPositionInPix, 4, 10, containerView);
        verify(mInputMethodManager).startStylusHandwriting(containerView);
    }

    @Test
    public void testStylusHandwritingLogsApiOption() {
        ViewGroup containerView = mock(ViewGroup.class);
        Rect boundsInPix = new Rect(20, 20, 80, 80);
        Point cursorPositionInPix = new Point(40, 40);
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "InputMethod.StylusHandwriting.Triggered", StylusApiOption.Api.ANDROID);
        mHandler.onEditElementFocusedForStylusWriting(
                boundsInPix, cursorPositionInPix, 4, 10, containerView);
        histogramWatcher.assertExpected();
    }

    @Test
    public void testShouldInitiateStylusWriting() {
        assertTrue(mHandler.shouldInitiateStylusWriting());
        verify(mInputMethodManager, never()).startStylusHandwriting(any());
    }

    @Test
    public void testHandlerUsesAndroidIcon() {
        assertEquals(TYPE_HANDWRITING, mHandler.getStylusPointerIcon());
    }
}
