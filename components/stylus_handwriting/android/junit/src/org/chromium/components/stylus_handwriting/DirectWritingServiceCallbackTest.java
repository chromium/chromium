// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.stylus_handwriting;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.robolectric.Shadows.shadowOf;

import android.content.Context;
import android.graphics.Point;
import android.graphics.PointF;
import android.graphics.Rect;
import android.os.Bundle;
import android.os.Looper;
import android.text.TextUtils;
import android.view.ViewGroup;
import android.view.inputmethod.EditorInfo;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.blink_public.web.WebTextInputFlags;
import org.chromium.blink_public.web.WebTextInputMode;
import org.chromium.content.browser.input.ImeUtils;
import org.chromium.content_public.browser.StylusWritingImeCallback;
import org.chromium.ui.base.ime.TextInputAction;
import org.chromium.ui.base.ime.TextInputType;

/**
 * Unit tests for {@link DirectWritingServiceCallback}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class DirectWritingServiceCallbackTest {
    private static final String SAMPLE_INPUT = "sample input";

    @Mock
    private StylusWritingImeCallback mImeCallback;
    @Mock
    private ViewGroup mContainerView;

    private DirectWritingServiceCallback mDwServiceCallback = new DirectWritingServiceCallback();
    private Context mContext;

    @Before
    public void setup() {
        MockitoAnnotations.initMocks(this);
        mContext = RuntimeEnvironment.application;
        doReturn(mContainerView).when(mImeCallback).getContainerView();
        doReturn(mContext).when(mContainerView).getContext();
    }

    @Test
    @Feature({"Stylus Handwriting"})
    public void testInputStateData() {
        // Input state values are default when updateInputState is not called yet.
        assertEquals("", mDwServiceCallback.getText());
        assertEquals(0, mDwServiceCallback.getSelectionStart());
        assertEquals(0, mDwServiceCallback.getSelectionEnd());

        // Set input state params and verify.
        int selectionStart = 2;
        int selectionEnd = 2;
        mDwServiceCallback.updateInputState(SAMPLE_INPUT, selectionStart, selectionEnd);
        assertEquals(SAMPLE_INPUT, mDwServiceCallback.getText());
        assertEquals(selectionStart, mDwServiceCallback.getSelectionStart());
        assertEquals(selectionEnd, mDwServiceCallback.getSelectionEnd());

        // Verify edit bounds rect and cursor location point.
        assertTrue(mDwServiceCallback.getCursorLocation(0).equals(0, 0));
        assertEquals(0, mDwServiceCallback.getLeft());
        assertEquals(0, mDwServiceCallback.getRight());
        assertEquals(0, mDwServiceCallback.getTop());
        assertEquals(0, mDwServiceCallback.getBottom());
        Rect editBounds = new Rect(10, 20, 100, 200);
        Point cursorLocation = new Point(10, 20);
        mDwServiceCallback.updateEditableBounds(editBounds, cursorLocation);
        assertEquals(new PointF(cursorLocation), mDwServiceCallback.getCursorLocation(0));
        assertEquals(editBounds.left, mDwServiceCallback.getLeft());
        assertEquals(editBounds.right, mDwServiceCallback.getRight());
        assertEquals(editBounds.top, mDwServiceCallback.getTop());
        assertEquals(editBounds.bottom, mDwServiceCallback.getBottom());
    }

    @Test
    @Feature({"Stylus Handwriting"})
    public void testHideKeyboardMessage() {
        // hide keyboard is called only after Ime callback is set.
        mDwServiceCallback.semForceHideSoftInput();
        shadowOf(Looper.getMainLooper()).idle();
        verify(mImeCallback, never()).hideKeyboard();

        mDwServiceCallback.setImeCallback(mImeCallback);
        mDwServiceCallback.semForceHideSoftInput();
        shadowOf(Looper.getMainLooper()).idle();
        verify(mImeCallback).hideKeyboard();
    }

    @Test
    @Feature({"Stylus Handwriting"})
    public void testShowKeyboardMessage() {
        String action = "show keyboard";
        // hide keyboard is called only after Ime callback is set.
        Bundle bundle = spy(new Bundle());
        bundle.putBoolean(DirectWritingServiceCallback.BUNDLE_KEY_SHOW_KEYBOARD, false);
        mDwServiceCallback.onAppPrivateCommand(action, bundle);
        shadowOf(Looper.getMainLooper()).idle();
        verify(mImeCallback, never()).getContainerView();
        verify(mImeCallback, never()).showSoftKeyboard();

        mDwServiceCallback.setImeCallback(mImeCallback);
        mDwServiceCallback.onAppPrivateCommand(action, bundle);
        shadowOf(Looper.getMainLooper()).idle();
        verify(mImeCallback).getContainerView();
        verify(bundle).getBoolean(DirectWritingServiceCallback.BUNDLE_KEY_SHOW_KEYBOARD);
        verify(mImeCallback, never()).showSoftKeyboard();

        bundle.putBoolean(DirectWritingServiceCallback.BUNDLE_KEY_SHOW_KEYBOARD, true);
        mDwServiceCallback.onAppPrivateCommand(action, bundle);
        shadowOf(Looper.getMainLooper()).idle();
        verify(mImeCallback).showSoftKeyboard();
    }

    @Test
    @Feature({"Stylus Handwriting"})
    public void testOnEditorActionMessage() {
        int editorAction = EditorInfo.IME_ACTION_SEARCH;
        // Editor action can be done only after Ime callback is set.
        mDwServiceCallback.onEditorAction(editorAction);
        shadowOf(Looper.getMainLooper()).idle();
        verify(mImeCallback, never()).performEditorAction(anyInt());

        mDwServiceCallback.setImeCallback(mImeCallback);
        mDwServiceCallback.onEditorAction(editorAction);
        shadowOf(Looper.getMainLooper()).idle();
        verify(mImeCallback).performEditorAction(editorAction);
    }

    @Test
    @Feature({"Stylus Handwriting"})
    public void testSetTextSelectionMessage() {
        // Text received from service is committed only after Ime callback is set.
        int index = SAMPLE_INPUT.length();
        mDwServiceCallback.setTextSelection(SAMPLE_INPUT, index);
        shadowOf(Looper.getMainLooper()).idle();
        verify(mImeCallback, never()).setEditableSelectionOffsets(anyInt(), anyInt());
        verify(mImeCallback, never()).sendCompositionToNative(any(), anyInt(), anyBoolean());

        // Text received from service replaces the current text in input.
        mDwServiceCallback.setImeCallback(mImeCallback);
        String currentInputText = "test";
        mDwServiceCallback.updateInputState(currentInputText, 4, 4);
        mDwServiceCallback.setTextSelection(SAMPLE_INPUT, index);
        shadowOf(Looper.getMainLooper()).idle();
        verify(mImeCallback).setEditableSelectionOffsets(0, currentInputText.length());
        verify(mImeCallback).sendCompositionToNative(SAMPLE_INPUT, index, true);
        verify(mImeCallback).setEditableSelectionOffsets(index, index);
    }

    @Test
    @Feature({"Stylus Handwriting"})
    public void testEditorInfoAttributes() {
        // Service callback returns default values until Editor info is set.
        assertTrue(TextUtils.isEmpty(mDwServiceCallback.getPrivateImeOptions()));
        assertEquals(0, mDwServiceCallback.getImeOptions());
        assertEquals(0, mDwServiceCallback.getInputType());

        EditorInfo editorInfo = new EditorInfo();
        int index = SAMPLE_INPUT.length();
        ImeUtils.computeEditorInfo(TextInputType.TEXT, WebTextInputFlags.NONE,
                WebTextInputMode.DEFAULT, TextInputAction.SEARCH, index, index, SAMPLE_INPUT,
                editorInfo);
        mDwServiceCallback.updateEditorInfo(editorInfo);
        assertEquals(editorInfo.privateImeOptions, mDwServiceCallback.getPrivateImeOptions());
        assertEquals(editorInfo.imeOptions, mDwServiceCallback.getImeOptions());
        assertEquals(editorInfo.inputType, mDwServiceCallback.getInputType());
    }
}
