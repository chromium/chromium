// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.stylus_handwriting;

import android.graphics.Point;
import android.graphics.Rect;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.view.inputmethod.EditorInfo;

import androidx.annotation.BinderThread;

import org.chromium.content_public.browser.StylusWritingImeCallback;

/**
 * This class implements the Direct Writing service callback interface that gets registered to the
 * service, which would be called on the {@link BinderThread}.
 */
class DirectWritingServiceCallback
        extends android.widget.directwriting.IDirectWritingServiceCallback.Stub {
    private static final String TAG = "DWCallback";

    private EditorInfo mEditorInfo;
    private int mLastSelectionStart;
    private int mLastSelectionEnd;
    private String mLastText;
    private Rect mEditableBounds;
    private Point mCursorPosition;

    private StylusWritingImeCallback mStylusWritingImeCallback;

    private final Handler mHandler = new Handler((android.os.Looper.getMainLooper())) {
        @Override
        public void handleMessage(Message msg) {
            if (mStylusWritingImeCallback == null) return;
            switch (msg.what) {
                case DirectWritingConstants.MSG_SEND_SET_TEXT_SELECTION:
                    mStylusWritingImeCallback.setEditableSelectionOffsets(0, mLastText.length());
                    mStylusWritingImeCallback.sendCompositionToNative(
                            ((CharSequence) msg.obj), msg.arg1, true);
                    mStylusWritingImeCallback.setEditableSelectionOffsets(msg.arg1, msg.arg1);
                    break;
                case DirectWritingConstants.MSG_PERFORM_EDITOR_ACTION:
                    mStylusWritingImeCallback.performEditorAction(msg.arg1);
                    break;
                case DirectWritingConstants.MSG_PERFORM_SHOW_KEYBOARD:
                    mStylusWritingImeCallback.showSoftKeyboard();
                    break;
                case DirectWritingConstants.MSG_TEXT_VIEW_EXTRA_COMMAND:
                    // TODO(mahesh.ma): Add DW recognized gesture handling logic here.
                    break;
                case DirectWritingConstants.MSG_FORCE_HIDE_KEYBOARD:
                    mStylusWritingImeCallback.hideKeyboard();
                    break;
                default:
                    break;
            }
        }
    };

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
