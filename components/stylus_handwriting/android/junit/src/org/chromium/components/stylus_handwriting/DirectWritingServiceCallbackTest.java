// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.stylus_handwriting;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
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
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.blink.mojom.StylusWritingGestureAction;
import org.chromium.blink.mojom.StylusWritingGestureData;
import org.chromium.blink_public.web.WebTextInputFlags;
import org.chromium.blink_public.web.WebTextInputMode;
import org.chromium.content.browser.input.ImeUtils;
import org.chromium.content_public.browser.StylusWritingImeCallback;
import org.chromium.mojo_base.mojom.String16;
import org.chromium.ui.base.ime.TextInputAction;
import org.chromium.ui.base.ime.TextInputType;

import java.util.Arrays;
import java.util.List;

/** Unit tests for {@link DirectWritingServiceCallback}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class DirectWritingServiceCallbackTest {
    private static final String SAMPLE_INPUT = "sample input";
    private static final String FALLBACK_TEXT = "fallback";
    private static final float[] GESTURE_START_POINT = new float[] {20.f, 50.f};
    private static final float[] GESTURE_END_POINT = new float[] {100.f, 50.f};
    private static final List<String> TWO_POINT_GESTURES =
            Arrays.asList(
                    DirectWritingServiceCallback.GESTURE_TYPE_ZIGZAG,
                    DirectWritingServiceCallback.GESTURE_TYPE_BACKSPACE,
                    DirectWritingServiceCallback.GESTURE_TYPE_U_TYPE_REMOVE_SPACE,
                    DirectWritingServiceCallback.GESTURE_TYPE_ARCH_TYPE_REMOVE_SPACE);

    @Mock private StylusWritingImeCallback mImeCallback;
    @Mock private ViewGroup mContainerView;

    private DirectWritingServiceCallback mDwServiceCallback = new DirectWritingServiceCallback();
    private Context mContext;

    private static String mojoStringToJavaString(String16 mojoString) {
        short[] data = mojoString.data;
        char[] chars = new char[data.length];
        for (int i = 0; i < chars.length; i++) {
            chars[i] = (char) data[i];
        }
        return String.valueOf(chars);
    }

    private static Bundle getGestureBundle(String gestureType) {
        Bundle bundle = new Bundle();
        bundle.putString(DirectWritingServiceCallback.GESTURE_BUNDLE_KEY_GESTURE_TYPE, gestureType);
        bundle.putString(
                DirectWritingServiceCallback.GESTURE_BUNDLE_KEY_TEXT_ALTERNATIVE, FALLBACK_TEXT);
        bundle.putFloatArray(getGestureStartPointKey(gestureType), GESTURE_START_POINT);

        if (isTwoPointGesture(gestureType)) {
            bundle.putFloatArray(
                    DirectWritingServiceCallback.GESTURE_BUNDLE_KEY_END_POINT, GESTURE_END_POINT);
        }
        return bundle;
    }

    private static boolean isTwoPointGesture(String gestureType) {
        return TWO_POINT_GESTURES.contains(gestureType);
    }

    private static String getGestureStartPointKey(String gestureType) {
        if (gestureType.equals(DirectWritingServiceCallback.GESTURE_TYPE_V_SPACE)) {
            return DirectWritingServiceCallback.GESTURE_BUNDLE_KEY_LOWEST_POINT;
        } else if (gestureType.equals(DirectWritingServiceCallback.GESTURE_TYPE_WEDGE_SPACE)) {
            return DirectWritingServiceCallback.GESTURE_BUNDLE_KEY_HIGHEST_POINT;
        } else if (gestureType.equals(DirectWritingServiceCallback.GESTURE_I_TYPE_FUNCTIONAL)) {
            return DirectWritingServiceCallback.GESTURE_BUNDLE_KEY_CENTER_POINT;
        } else {
            return DirectWritingServiceCallback.GESTURE_BUNDLE_KEY_START_POINT;
        }
    }

    private void setImeCallbackAndVerifyMojoGestureData(
            Bundle gestureBundle,
            @StylusWritingGestureAction.EnumType int expectedAction,
            String expectedTextToInsert) {
        mDwServiceCallback.updateEditableBounds(new Rect(0, 0, 400, 400), new Point(50, 50));
        mDwServiceCallback.setImeCallback(mImeCallback);
        mDwServiceCallback.onTextViewExtraCommand(
                DirectWritingServiceCallback.GESTURE_ACTION_RECOGNITION_INFO, gestureBundle);
        shadowOf(Looper.getMainLooper()).idle();
        ArgumentCaptor<StylusWritingGestureData> gestureDataCaptor =
                ArgumentCaptor.forClass(StylusWritingGestureData.class);
        verify(mImeCallback)
                .handleStylusWritingGestureAction(anyInt(), gestureDataCaptor.capture());
        StylusWritingGestureData gestureData = gestureDataCaptor.getValue();
        assertEquals(expectedAction, gestureData.action);
        assertEquals(GESTURE_START_POINT[0], gestureData.startRect.x, /* tolerance= */ 0.1);
        assertEquals(GESTURE_START_POINT[1], gestureData.startRect.y, /* tolerance= */ 0.1);

        if (isTwoPointGesture(
                gestureBundle.getString(
                        DirectWritingServiceCallback.GESTURE_BUNDLE_KEY_GESTURE_TYPE, ""))) {
            assertEquals(GESTURE_END_POINT[0], gestureData.endRect.x, /* tolerance= */ 0.1);
            assertEquals(GESTURE_END_POINT[1], gestureData.endRect.y, /* tolerance= */ 0.1);
        } else {
            assertNull(gestureData.endRect);
        }

        assertEquals(FALLBACK_TEXT, mojoStringToJavaString(gestureData.textAlternative));
        if (expectedTextToInsert == null) {
            assertNull(gestureData.textToInsert);
        } else {
            assertEquals(expectedTextToInsert, mojoStringToJavaString(gestureData.textToInsert));
        }
    }

    private void sendGestureAndVerifyGestureNotHandled(Bundle bundle) {
        mDwServiceCallback.onTextViewExtraCommand(
                DirectWritingServiceCallback.GESTURE_ACTION_RECOGNITION_INFO, bundle);
        shadowOf(Looper.getMainLooper()).idle();
        verify(mImeCallback, never()).handleStylusWritingGestureAction(anyInt(), any());
    }

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
        verify(mImeCallback, never()).finishComposingText();

        // Text received from service replaces the current text in input.
        mDwServiceCallback.setImeCallback(mImeCallback);
        String currentInputText = "test";
        mDwServiceCallback.updateInputState(currentInputText, 4, 4);
        mDwServiceCallback.setTextSelection(SAMPLE_INPUT, index);
        shadowOf(Looper.getMainLooper()).idle();
        verify(mImeCallback).finishComposingText();
        verify(mImeCallback).setEditableSelectionOffsets(0, currentInputText.length());
        verify(mImeCallback).sendCompositionToNative(SAMPLE_INPUT, index, true);
        verify(mImeCallback).setEditableSelectionOffsets(index, index);
    }

    @Test
    @Feature({"Stylus Handwriting"})
    public void testUpdateEditBoundsMessage() {
        mDwServiceCallback.setImeCallback(mImeCallback);
        DirectWritingServiceCallback.TriggerCallback mockTriggercallback =
                mock(DirectWritingServiceCallback.TriggerCallback.class);
        mDwServiceCallback.setTriggerCallback(mockTriggercallback);
        mDwServiceCallback.updateBoundedEditTextRect();
        shadowOf(Looper.getMainLooper()).idle();
        verify(mockTriggercallback).updateEditableBoundsToService();
    }

    @Test
    @Feature({"Stylus Handwriting"})
    public void testIsHoverIconShowing() {
        DirectWritingServiceCallback.TriggerCallback mockTriggercallback =
                mock(DirectWritingServiceCallback.TriggerCallback.class);
        mDwServiceCallback.setTriggerCallback(mockTriggercallback);
        assertFalse(mDwServiceCallback.isHoverIconShowing());
        doReturn(true).when(mockTriggercallback).isHandwritingIconShowing();
        assertTrue(mDwServiceCallback.isHoverIconShowing());
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
        ImeUtils.computeEditorInfo(
                TextInputType.TEXT,
                WebTextInputFlags.NONE,
                WebTextInputMode.DEFAULT,
                TextInputAction.SEARCH,
                index,
                index,
                SAMPLE_INPUT,
                editorInfo);
        mDwServiceCallback.updateEditorInfo(editorInfo);
        assertEquals(editorInfo.privateImeOptions, mDwServiceCallback.getPrivateImeOptions());
        assertEquals(editorInfo.imeOptions, mDwServiceCallback.getImeOptions());
        assertEquals(editorInfo.inputType, mDwServiceCallback.getInputType());
    }

    @Test
    @Feature({"Stylus Handwriting"})
    public void testStylusGestureMessage_deleteByStrike() {
        // Stylus gesture Delete is handled only after Ime callback is set.
        Bundle bundle = getGestureBundle(DirectWritingServiceCallback.GESTURE_TYPE_BACKSPACE);
        sendGestureAndVerifyGestureNotHandled(bundle);

        setImeCallbackAndVerifyMojoGestureData(
                bundle, StylusWritingGestureAction.DELETE_TEXT, null);
    }

    @Test
    @Feature({"Stylus Handwriting"})
    public void testStylusGestureMessage_deleteByScribble() {
        // Stylus gesture Delete is handled only after Ime callback is set.
        Bundle bundle = getGestureBundle(DirectWritingServiceCallback.GESTURE_TYPE_ZIGZAG);
        sendGestureAndVerifyGestureNotHandled(bundle);

        setImeCallbackAndVerifyMojoGestureData(
                bundle, StylusWritingGestureAction.DELETE_TEXT, null);
    }

    @Test
    @Feature({"Stylus Handwriting"})
    public void testStylusGestureMessage_deletePointsClampedToEditBounds() {
        mDwServiceCallback.setImeCallback(mImeCallback);
        // Set Edit field bounds smaller than gesture x-coordinates.
        Rect editBounds = new Rect(30, 30, 80, 80);
        mDwServiceCallback.updateEditableBounds(editBounds, new Point(50, 50));

        Bundle bundle = getGestureBundle(DirectWritingServiceCallback.GESTURE_TYPE_BACKSPACE);
        mDwServiceCallback.onTextViewExtraCommand(
                DirectWritingServiceCallback.GESTURE_ACTION_RECOGNITION_INFO, bundle);
        shadowOf(Looper.getMainLooper()).idle();
        verify(mImeCallback)
                .handleStylusWritingGestureAction(
                        anyInt(),
                        argThat(
                                gestureData -> {
                                    assertEquals(
                                            StylusWritingGestureAction.DELETE_TEXT,
                                            gestureData.action);
                                    // assert that start-x and end-x are clamped to Edit bounds.
                                    assertEquals(editBounds.left, gestureData.startRect.x);
                                    assertEquals(editBounds.right, gestureData.endRect.x);
                                    return true;
                                }));
    }

    @Test
    @Feature({"Stylus Handwriting"})
    public void testStylusGestureMessage_insertVSpace() {
        // Stylus gesture add space is handled only after Ime callback is set.
        Bundle bundle = getGestureBundle(DirectWritingServiceCallback.GESTURE_TYPE_V_SPACE);
        sendGestureAndVerifyGestureNotHandled(bundle);

        setImeCallbackAndVerifyMojoGestureData(
                bundle, StylusWritingGestureAction.ADD_SPACE_OR_TEXT, " ");
    }

    @Test
    @Feature({"Stylus Handwriting"})
    public void testStylusGestureMessage_insertVText() {
        Bundle bundle = getGestureBundle(DirectWritingServiceCallback.GESTURE_TYPE_V_SPACE);
        // Set text to be inserted with V gesture.
        String textToInsert = "text";
        bundle.putString(
                DirectWritingServiceCallback.GESTURE_BUNDLE_KEY_TEXT_INSERTION, textToInsert);
        setImeCallbackAndVerifyMojoGestureData(
                bundle, StylusWritingGestureAction.ADD_SPACE_OR_TEXT, textToInsert);
    }

    @Test
    @Feature({"Stylus Handwriting"})
    public void testStylusGestureMessage_insertWedgeSpace() {
        // Stylus gesture add wedge space is handled only after Ime callback is set.
        Bundle bundle = getGestureBundle(DirectWritingServiceCallback.GESTURE_TYPE_WEDGE_SPACE);
        sendGestureAndVerifyGestureNotHandled(bundle);

        setImeCallbackAndVerifyMojoGestureData(
                bundle, StylusWritingGestureAction.ADD_SPACE_OR_TEXT, " ");
    }

    @Test
    @Feature({"Stylus Handwriting"})
    public void testStylusGestureMessage_insertWedgeText() {
        Bundle bundle = getGestureBundle(DirectWritingServiceCallback.GESTURE_TYPE_WEDGE_SPACE);
        // Set text to be inserted with wedge gesture.
        String textToInsert = "text";
        bundle.putString(
                DirectWritingServiceCallback.GESTURE_BUNDLE_KEY_TEXT_INSERTION, textToInsert);
        setImeCallbackAndVerifyMojoGestureData(
                bundle, StylusWritingGestureAction.ADD_SPACE_OR_TEXT, textToInsert);
    }

    @Test
    @Feature({"Stylus Handwriting"})
    public void testStylusGestureMessage_splitOrMerge() {
        // Stylus gesture split or merge is handled only after Ime callback is set.
        Bundle bundle = getGestureBundle(DirectWritingServiceCallback.GESTURE_I_TYPE_FUNCTIONAL);
        sendGestureAndVerifyGestureNotHandled(bundle);

        setImeCallbackAndVerifyMojoGestureData(
                bundle, StylusWritingGestureAction.SPLIT_OR_MERGE, null);
    }

    @Test
    @Feature({"Stylus Handwriting"})
    public void testStylusGestureMessage_unSupportedWithFallbackText() {
        // Invalid gesture commits fallback text.
        Bundle bundle = new Bundle();
        bundle.putString(DirectWritingServiceCallback.GESTURE_BUNDLE_KEY_GESTURE_TYPE, "invalid");
        bundle.putString(
                DirectWritingServiceCallback.GESTURE_BUNDLE_KEY_TEXT_ALTERNATIVE, FALLBACK_TEXT);
        sendGestureAndVerifyGestureNotHandled(bundle);

        mDwServiceCallback.updateEditableBounds(new Rect(0, 0, 400, 400), new Point(50, 50));
        mDwServiceCallback.setImeCallback(mImeCallback);
        mDwServiceCallback.onTextViewExtraCommand(
                DirectWritingServiceCallback.GESTURE_ACTION_RECOGNITION_INFO, bundle);
        shadowOf(Looper.getMainLooper()).idle();
        verify(mImeCallback).sendCompositionToNative(FALLBACK_TEXT, FALLBACK_TEXT.length(), true);
        verify(mImeCallback, never()).handleStylusWritingGestureAction(anyInt(), any());
    }

    @Test
    @Feature({"Stylus Handwriting"})
    public void testStylusGestureMessage_UTypeRemoveSpaces() {
        // Stylus gesture remove spaces is handled only after Ime callback is set.
        Bundle bundle =
                getGestureBundle(DirectWritingServiceCallback.GESTURE_TYPE_U_TYPE_REMOVE_SPACE);
        sendGestureAndVerifyGestureNotHandled(bundle);

        setImeCallbackAndVerifyMojoGestureData(
                bundle, StylusWritingGestureAction.REMOVE_SPACES, null);
    }

    @Test
    @Feature({"Stylus Handwriting"})
    public void testStylusGestureMessage_archTypeRemoveSpaces() {
        // Stylus gesture remove spaces is handled only after Ime callback is set.
        Bundle bundle =
                getGestureBundle(DirectWritingServiceCallback.GESTURE_TYPE_ARCH_TYPE_REMOVE_SPACE);
        sendGestureAndVerifyGestureNotHandled(bundle);

        setImeCallbackAndVerifyMojoGestureData(
                bundle, StylusWritingGestureAction.REMOVE_SPACES, null);
    }

    @Test
    @Feature({"Stylus Handwriting"})
    public void testStylusGestureMessage_invalidGestureTypeWithoutFallbackText() {
        mDwServiceCallback.setImeCallback(mImeCallback);
        // Gesture type other than the expected ones are not handled.
        Bundle bundle = spy(new Bundle());
        bundle.putString(
                DirectWritingServiceCallback.GESTURE_BUNDLE_KEY_GESTURE_TYPE, "invalid_gesture");
        // verify that gesture bundle is accessed but gesture is not handled for invalid gesture.
        sendGestureAndVerifyGestureNotHandled(bundle);
        verify(mImeCallback, never()).sendCompositionToNative(any(), anyInt(), anyBoolean());
        verify(bundle).getString(DirectWritingServiceCallback.GESTURE_BUNDLE_KEY_GESTURE_TYPE, "");
    }

    @Test
    @Feature({"Stylus Handwriting"})
    public void testOnTextViewExtraCommand_invalidAction() {
        mDwServiceCallback.setImeCallback(mImeCallback);
        // Text view extra command only handles gesture recognition. Other actions are ignored.
        Bundle bundle =
                spy(
                        getGestureBundle(
                                DirectWritingServiceCallback.GESTURE_TYPE_ARCH_TYPE_REMOVE_SPACE));
        mDwServiceCallback.onTextViewExtraCommand("invalid", bundle);
        shadowOf(Looper.getMainLooper()).idle();
        // verify that Gesture bundle is never accessed, and not handled for invalid action.
        verify(bundle, never()).getString(any(), any());
        verify(mImeCallback, never()).handleStylusWritingGestureAction(anyInt(), any());
    }
}
