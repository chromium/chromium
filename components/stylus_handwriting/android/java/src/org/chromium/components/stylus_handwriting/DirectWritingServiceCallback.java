// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.stylus_handwriting;

import android.content.Context;
import android.graphics.Point;
import android.graphics.PointF;
import android.graphics.Rect;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.text.TextUtils;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;

import androidx.annotation.BinderThread;

import org.chromium.base.Log;
import org.chromium.base.MathUtils;
import org.chromium.blink.mojom.StylusWritingGestureAction;
import org.chromium.blink.mojom.StylusWritingGestureData;
import org.chromium.blink.mojom.StylusWritingGestureGranularity;
import org.chromium.content.browser.input.StylusGestureConverter;
import org.chromium.content_public.browser.StylusWritingImeCallback;
import org.chromium.mojo_base.mojom.String16;

/**
 * This class implements the Direct Writing service callback interface that gets registered to the
 * service, which would be called on the {@link BinderThread}. It also caches information about
 * writable element position, cursor position and the text input state to be provided to the service
 * when requested on the {@link BinderThread}.
 */
class DirectWritingServiceCallback
        extends android.widget.directwriting.IDirectWritingServiceCallback.Stub {
    static final String BUNDLE_KEY_SHOW_KEYBOARD = "showKeyboard";
    private static final String TAG = "DWCallbackImpl";

    // The following GESTURE_ and ACTION_ constants are defined as per the bundle data sent by the
    // Direct Writing service when any gesture is recognized.
    static final String GESTURE_ACTION_RECOGNITION_INFO = "recognition_info";
    static final String GESTURE_BUNDLE_KEY_START_POINT = "start_point";
    static final String GESTURE_BUNDLE_KEY_END_POINT = "end_point";
    static final String GESTURE_BUNDLE_KEY_LOWEST_POINT = "lowest_point";
    static final String GESTURE_BUNDLE_KEY_HIGHEST_POINT = "highest_point";
    static final String GESTURE_BUNDLE_KEY_CENTER_POINT = "center_point";
    static final String GESTURE_BUNDLE_KEY_GESTURE_TYPE = "gesture_type";
    static final String GESTURE_BUNDLE_KEY_TEXT_ALTERNATIVE = "text_alternative";
    static final String GESTURE_BUNDLE_KEY_TEXT_INSERTION = "text_insertion";

    // Gesture types in Bundle for GESTURE_BUNDLE_KEY_GESTURE_TYPE
    static final String GESTURE_TYPE_BACKSPACE = "backspace";
    static final String GESTURE_TYPE_ZIGZAG = "zigzag";
    static final String GESTURE_TYPE_V_SPACE = "v_space";
    static final String GESTURE_TYPE_WEDGE_SPACE = "wedge_space";
    static final String GESTURE_TYPE_U_TYPE_REMOVE_SPACE = "u_type_remove_space";
    static final String GESTURE_TYPE_ARCH_TYPE_REMOVE_SPACE = "arch_type_remove_space";
    static final String GESTURE_I_TYPE_FUNCTIONAL = "i_type_functional";

    /** Callback interface for DirectWritingTrigger class. */
    public interface TriggerCallback {
        /** Update editable bounds to Direct Writing service. */
        void updateEditableBoundsToService();

        /** @return true if stylus handwriting icon is showing, false otherwise. */
        boolean isHandwritingIconShowing();
    }

    private EditorInfo mEditorInfo;
    private int mLastSelectionStart;
    private int mLastSelectionEnd;
    private String mLastText;
    private Rect mEditableBounds;
    private Point mCursorPosition;

    private StylusWritingImeCallback mStylusWritingImeCallback;
    private TriggerCallback mTriggerCallback;

    void setTriggerCallback(TriggerCallback callback) {
        mTriggerCallback = callback;
    }

    private final Handler mHandler =
            new Handler(android.os.Looper.getMainLooper()) {
                @Override
                public void handleMessage(Message msg) {
                    if (mStylusWritingImeCallback == null) return;
                    switch (msg.what) {
                        case DirectWritingConstants.MSG_SEND_SET_TEXT_SELECTION:
                            mStylusWritingImeCallback.finishComposingText();
                            mStylusWritingImeCallback.setEditableSelectionOffsets(
                                    0, getText().length());
                            mStylusWritingImeCallback.sendCompositionToNative(
                                    ((CharSequence) msg.obj), msg.arg1, true);
                            mStylusWritingImeCallback.setEditableSelectionOffsets(
                                    msg.arg1, msg.arg1);
                            break;
                        case DirectWritingConstants.MSG_PERFORM_EDITOR_ACTION:
                            mStylusWritingImeCallback.performEditorAction(msg.arg1);
                            break;
                        case DirectWritingConstants.MSG_PERFORM_SHOW_KEYBOARD:
                            mStylusWritingImeCallback.showSoftKeyboard();
                            break;
                        case DirectWritingConstants.MSG_TEXT_VIEW_EXTRA_COMMAND:
                            String action = (String) msg.obj;
                            if (action.equals(GESTURE_ACTION_RECOGNITION_INFO)) {
                                Bundle gestureBundle = msg.getData();
                                handleDwGesture(gestureBundle);
                            }
                            break;
                        case DirectWritingConstants.MSG_FORCE_HIDE_KEYBOARD:
                            mStylusWritingImeCallback.hideKeyboard();
                            break;
                        case DirectWritingConstants.MSG_UPDATE_EDIT_BOUNDS:
                            mTriggerCallback.updateEditableBoundsToService();
                            break;
                        default:
                            break;
                    }
                }
            };

    private void handleDwGesture(Bundle bundle) {
        if (mStylusWritingImeCallback == null) return;
        String gestureType = bundle.getString(GESTURE_BUNDLE_KEY_GESTURE_TYPE, "");
        Log.d(TAG, "Received Direct Writing gesture of type: " + gestureType);
        if (TextUtils.isEmpty(gestureType)) return;

        // When the gesture recognized is not at a valid character position in the HTML input field,
        // then the text alternative would be inserted at the current cursor position.
        String textAlternative = bundle.getString(GESTURE_BUNDLE_KEY_TEXT_ALTERNATIVE, "");
        float[] startPoint;

        // Populate gesture data as applicable for different gestures recognized.
        StylusWritingGestureData gestureData = new StylusWritingGestureData();

        gestureData.granularity = StylusWritingGestureGranularity.CHARACTER;
        if (gestureType.equals(GESTURE_TYPE_BACKSPACE) || gestureType.equals(GESTURE_TYPE_ZIGZAG)) {
            startPoint = bundle.getFloatArray(GESTURE_BUNDLE_KEY_START_POINT);
            gestureData.endRect =
                    mojoRectClampedToEditableBounds(
                            bundle.getFloatArray(GESTURE_BUNDLE_KEY_END_POINT));
            gestureData.action = StylusWritingGestureAction.DELETE_TEXT;
        } else if (gestureType.equals(GESTURE_TYPE_V_SPACE)) {
            startPoint = bundle.getFloatArray(GESTURE_BUNDLE_KEY_LOWEST_POINT);
            populateDataForAddSpaceOrTextGesture(gestureData, bundle);
        } else if (gestureType.equals(GESTURE_TYPE_WEDGE_SPACE)) {
            startPoint = bundle.getFloatArray(GESTURE_BUNDLE_KEY_HIGHEST_POINT);
            populateDataForAddSpaceOrTextGesture(gestureData, bundle);
        } else if (gestureType.equals(GESTURE_TYPE_U_TYPE_REMOVE_SPACE)
                || gestureType.equals(GESTURE_TYPE_ARCH_TYPE_REMOVE_SPACE)) {
            startPoint = bundle.getFloatArray(GESTURE_BUNDLE_KEY_START_POINT);
            gestureData.endRect =
                    mojoRectClampedToEditableBounds(
                            bundle.getFloatArray(GESTURE_BUNDLE_KEY_END_POINT));
            gestureData.action = StylusWritingGestureAction.REMOVE_SPACES;
        } else if (gestureType.equals(GESTURE_I_TYPE_FUNCTIONAL)) {
            startPoint = bundle.getFloatArray(GESTURE_BUNDLE_KEY_CENTER_POINT);
            gestureData.action = StylusWritingGestureAction.SPLIT_OR_MERGE;
        } else {
            // Not an expected gesture.
            if (!TextUtils.isEmpty(textAlternative)) {
                // Commit fallback text if available for unsupported gesture. This is to provide
                // default behaviour for any unsupported gesture which is yet to be implemented.
                Log.d(TAG, "Commit fallback text for unsupported gesture: " + gestureType);
                mStylusWritingImeCallback.sendCompositionToNative(
                        textAlternative, textAlternative.length(), /* isCommit= */ true);
            } else {
                Log.w(TAG, "Skip handling unsupported gesture: " + gestureType);
            }
            return;
        }

        switch (gestureData.action) {
            case StylusWritingGestureAction.DELETE_TEXT:
                StylusGestureConverter.logGestureType(
                        StylusGestureConverter.UmaGestureType.DW_DELETE_TEXT);
                break;
            case StylusWritingGestureAction.ADD_SPACE_OR_TEXT:
                StylusGestureConverter.logGestureType(
                        StylusGestureConverter.UmaGestureType.DW_ADD_SPACE_OR_TEXT);
                break;
            case StylusWritingGestureAction.REMOVE_SPACES:
                StylusGestureConverter.logGestureType(
                        StylusGestureConverter.UmaGestureType.DW_REMOVE_SPACES);
                break;
            case StylusWritingGestureAction.SPLIT_OR_MERGE:
                StylusGestureConverter.logGestureType(
                        StylusGestureConverter.UmaGestureType.DW_SPLIT_OR_MERGE);
                break;
            default:
                assert false : "Gesture type unset";
        }

        // Populate the common data for all the gestures.
        gestureData.startRect = mojoRectClampedToEditableBounds(startPoint);
        gestureData.textAlternative = javaStringToMojoString(textAlternative);

        // Pass an ID of -1 as we don't care about the result of a gesture.
        mStylusWritingImeCallback.handleStylusWritingGestureAction(-1, gestureData);
    }

    private org.chromium.gfx.mojom.Rect mojoRectClampedToEditableBounds(float[] gesturePoint) {
        // Clamp gesture point to Editable bounds so that gesture is within editable area while
        // finding the gesture offset in blink, and to avoid hitting DCHECKs while doing so.
        float[] adjustedPoint = new float[2];
        adjustedPoint[0] =
                MathUtils.clamp(gesturePoint[0], mEditableBounds.left, mEditableBounds.right);
        adjustedPoint[1] =
                MathUtils.clamp(gesturePoint[1], mEditableBounds.top, mEditableBounds.bottom);
        return toMojoZeroSizeRect(adjustedPoint);
    }

    private static org.chromium.gfx.mojom.Rect toMojoZeroSizeRect(float[] gesturePoint) {
        org.chromium.gfx.mojom.Rect rect = new org.chromium.gfx.mojom.Rect();
        rect.x = (int) gesturePoint[0];
        rect.y = (int) gesturePoint[1];
        rect.width = 0;
        rect.height = 0;
        return rect;
    }

    private static void populateDataForAddSpaceOrTextGesture(
            StylusWritingGestureData gestureData, Bundle gestureBundle) {
        String textToInsert = gestureBundle.getString(GESTURE_BUNDLE_KEY_TEXT_INSERTION, "");
        // Insert space character when text to insert is empty inside gesture.
        if (TextUtils.isEmpty(textToInsert)) textToInsert = " ";

        gestureData.textToInsert = javaStringToMojoString(textToInsert);
        gestureData.action = StylusWritingGestureAction.ADD_SPACE_OR_TEXT;
    }

    private static String16 javaStringToMojoString(String string) {
        short[] data = new short[string.length()];
        for (int i = 0; i < data.length; i++) {
            data[i] = (short) string.charAt(i);
        }
        String16 mojoString = new String16();
        mojoString.data = data;
        return mojoString;
    }

    void updateInputState(String text, int selectionStart, int selectionEnd) {
        mLastText = text;
        mLastSelectionStart = selectionStart;
        mLastSelectionEnd = selectionEnd;
    }

    void updateEditorInfo(EditorInfo editorInfo) {
        mEditorInfo = editorInfo;
    }

    void updateEditableBounds(Rect editBounds, Point cursorPosition) {
        mEditableBounds = editBounds;
        mCursorPosition = cursorPosition;
    }

    void setImeCallback(StylusWritingImeCallback imeCallback) {
        mStylusWritingImeCallback = imeCallback;
    }

    // Implement the methods from IDirectWritingServiceCallback which are of interest for Chromium.
    // These are used to commit the recognized text and handle the gestures detected by the service.
    // They also provide information about current edit field position, cursor position and the
    // text input state.
    // HTML input text and selection setter APIs
    @BinderThread
    @Override
    public void setTextSelection(CharSequence text, int index) {
        Message msg = mHandler.obtainMessage(DirectWritingConstants.MSG_SEND_SET_TEXT_SELECTION);
        msg.obj = text;
        msg.arg1 = index;
        mHandler.sendMessage(msg);
    }

    // HTML input selection getters
    @BinderThread
    @Override
    public int getSelectionStart() {
        return mLastSelectionStart;
    }

    @BinderThread
    @Override
    public int getSelectionEnd() {
        return mLastSelectionEnd;
    }

    @BinderThread
    @Override
    public CharSequence getText() {
        if (mLastText == null) return "";
        return mLastText;
    }

    // HTML input field position getters
    @BinderThread
    @Override
    public int getRight() {
        return mEditableBounds != null ? mEditableBounds.right : 0;
    }

    @BinderThread
    @Override
    public int getLeft() {
        return mEditableBounds != null ? mEditableBounds.left : 0;
    }

    @BinderThread
    @Override
    public int getTop() {
        return mEditableBounds != null ? mEditableBounds.top : 0;
    }

    @BinderThread
    @Override
    public int getBottom() {
        return mEditableBounds != null ? mEditableBounds.bottom : 0;
    }

    // Direct Writing commit VI to Cursor location.
    @BinderThread
    @Override
    public PointF getCursorLocation(int selectionStart) {
        PointF cursorLocation =
                mCursorPosition == null ? new PointF() : new PointF(mCursorPosition);
        return cursorLocation;
    }

    // HTML Input EditInfo getters
    @BinderThread
    @Override
    public String getPrivateImeOptions() {
        String privateImeOptions = mEditorInfo != null ? mEditorInfo.privateImeOptions : "";
        return privateImeOptions;
    }

    @BinderThread
    @Override
    public int getImeOptions() {
        int imeOptions = mEditorInfo != null ? mEditorInfo.imeOptions : 0;
        return imeOptions;
    }

    @BinderThread
    @Override
    public int getInputType() {
        int inputType = mEditorInfo != null ? mEditorInfo.inputType : 0;
        return inputType;
    }

    @BinderThread
    @Override
    public void onEditorAction(int actionCode) {
        Message msg = mHandler.obtainMessage(DirectWritingConstants.MSG_PERFORM_EDITOR_ACTION);
        msg.arg1 = actionCode;
        mHandler.sendMessage(msg);
    }

    @BinderThread
    @Override
    public void onAppPrivateCommand(String action, Bundle bundle) {
        if (bundle == null || mStylusWritingImeCallback == null) return;
        View currentView = mStylusWritingImeCallback.getContainerView();
        if (currentView == null) return;
        InputMethodManager imm =
                (InputMethodManager)
                        currentView.getContext().getSystemService(Context.INPUT_METHOD_SERVICE);
        if (imm == null) return;
        imm.sendAppPrivateCommand(currentView, action, bundle);
        boolean showKeyboard = bundle.getBoolean(BUNDLE_KEY_SHOW_KEYBOARD);
        if (showKeyboard) {
            Message msg = mHandler.obtainMessage(DirectWritingConstants.MSG_PERFORM_SHOW_KEYBOARD);
            mHandler.sendMessage(msg);
        }
    }

    @BinderThread
    @Override
    public void semForceHideSoftInput() {
        Message msg = mHandler.obtainMessage(DirectWritingConstants.MSG_FORCE_HIDE_KEYBOARD);
        mHandler.sendMessage(msg);
    }

    // This API is used to receive the DW gesture type and gesture co-ordinates.
    @BinderThread
    @Override
    public void onTextViewExtraCommand(String action, Bundle bundle) {
        Message msg = mHandler.obtainMessage(DirectWritingConstants.MSG_TEXT_VIEW_EXTRA_COMMAND);
        msg.obj = action;
        msg.setData(bundle);
        mHandler.sendMessage(msg);
    }

    @BinderThread
    @Override
    public void updateBoundedEditTextRect() {
        Message msg = mHandler.obtainMessage(DirectWritingConstants.MSG_UPDATE_EDIT_BOUNDS);
        mHandler.sendMessage(msg);
    }

    @BinderThread
    @Override
    public boolean isHoverIconShowing() {
        return mTriggerCallback.isHandwritingIconShowing();
    }

    // All of the below IDirectWritingServiceCallback interface implementations are default or no-op
    // as they are not applicable to HTML inputs (or) we cannot provide these information in real
    // time as per current Chromium architecture.
    @Override
    public int getVersion() {
        return VERSION;
    }

    @Override
    public void onFinishRecognition() {}

    @Override
    public void bindEditIn(float x, float y) {}

    @Override
    public void setText(CharSequence text) {}

    @Override
    public void setSelection(int selection) {}

    @Override
    public int getOffsetForPosition(float x, float y) {
        return 0;
    }

    @Override
    public int length() {
        return 0;
    }

    @Override
    public int getHeight() {
        return 0;
    }

    @Override
    public int getWidth() {
        return 0;
    }

    @Override
    public int getScrollY() {
        return 0;
    }

    @Override
    public int getPaddingStart() {
        return 0;
    }

    @Override
    public int getPaddingTop() {
        return 0;
    }

    @Override
    public int getPaddingBottom() {
        return 0;
    }

    @Override
    public int getPaddingEnd() {
        return 0;
    }

    @Override
    public int getLineHeight() {
        return 0;
    }

    @Override
    public int getLineCount() {
        return 0;
    }

    @Override
    public int getBaseLine() {
        return 0;
    }

    @Override
    public int getParagraphDirection(int line) {
        return 0;
    }

    @Override
    public float getPrimaryHorizontal(int offset) {
        return 0;
    }

    @Override
    public float getLineMax(int i) {
        return 0;
    }

    @Override
    public int getLineForOffset(int offset) {
        return 0;
    }

    @Override
    public int getLineStart(int line) {
        return 0;
    }

    @Override
    public int getLineEnd(int line) {
        return 0;
    }

    @Override
    public int getLineTop(int line) {
        return 0;
    }

    @Override
    public int getLineBottom(int line) {
        return 0;
    }

    @Override
    public int getLineVisibleEnd(int line) {
        return 0;
    }

    @Override
    public int getLineBaseline(int line) {
        return 0;
    }

    @Override
    public int getLineHeightByIndex(int line) {
        return 0;
    }

    @Override
    public int getLineMaxIncludePadding(int line) {
        return 0;
    }

    @Override
    public int getLineAscent(int line) {
        return 0;
    }

    @Override
    public int getLineDescent(int line) {
        return 0;
    }

    @Override
    public Rect getTextAreaRect(int line) {
        return new Rect(0, 0, 0, 0);
    }

    @Override
    public void onExtraCommand(String action, Bundle bundle) {}
}
