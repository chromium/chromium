// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package android.widget.directwriting;

import android.os.Bundle;
import android.graphics.PointF;
import android.graphics.Rect;

/**
 * Direct writing service callback API for providing functionality of service callback operations,
 * mainly committing recognized and handling detected gestures. These APIs are to be implemented by
 * application making use of the direct writing service to:
 * 1. provide requested information regarding the current input field location and input state for
 *    which the writing is initiated.
 * 2. commit recognized text and set the cursor after commit.
 * 3. handle recognized gestures like deleting or adding text, spaces by using the gesture data
 *    Bundle provided by the service.
 */
interface IDirectWritingServiceCallback {
    const int VERSION = 3;

    /**
    * Gets version of Aidl
    *
    * @returns version IDirectWritingServiceCallback.VERSION
    */
    int getVersion() = 0;

    // { Life Cycle
    /**
    * Calls When finish writing from service
    */
    void onFinishRecognition() = 1;
    // Life Cycle}

    // { Bound EditText
    /**
    * Calls When Touch Up event happens outside of bounded edit text
    * Try to find and bind proper edit text near event position
    * Should run on main looper
    */
    void bindEditIn(float x, float y) = 10;

    /**
    * Called when the Service needs updated Edit field position. Used when the soft keyboard is
    * being shown or hidden to position the Direct writing widget toolbar near edit bounds.
    * This should respond by calling IDirectWritingService.onBoundedEditTextChanged with updated
    * bounds of focused edit field.
    * Should run on main looper.
    */
    void updateBoundedEditTextRect() = 11;
    // Bound EditText }

    // { EditText Text and Selection Setter
    /**
    * Sets text and cursor in bounded edit text
    * Should run on main looper
    *
    * @param text to set in edit text
    * @param index to set cursor position in edit text
    */
    void setTextSelection(CharSequence text, int index) = 20;
    /**
    * Same method with EditText
    * Should run on main looper
    */
    void setText(CharSequence text) = 21;
    /**
    * Same method with EditText
    * Should run on main looper
    */
    void setSelection(int selection) = 22;
    // EditText Text and Selection Setter }

    // { EditText Text and Selection Getter
    /**
    * Same method with EditText
    */
    int getSelectionStart() = 30;
    /**
    * Same method with EditText
    */
    int getSelectionEnd() = 31;
    /**
    * Same method with EditText
    */
    int getOffsetForPosition(float x, float y) = 32;
    /**
    * Same method with EditText
    */
    CharSequence getText() = 33;
    /**
    * Same method with EditText
    */
    int length() = 34;
    // EditText Text and Selection Getter }

    // EditText Rect and Size, Position Getter }
    /**
    * Same method with EditText
    */
    int getHeight() = 40;
    /**
    * Same method with EditText
    */
    int getWidth() = 41;
    /**
    * Same method with EditText
    */
    int getScrollY() = 42;
    /**
    * Same method with EditText
    */
    int getPaddingStart() = 43;
    /**
    * Same method with EditText
    */
    int getPaddingTop() = 44;
    /**
    * Same method with EditText
    */
    int getPaddingBottom() = 45;
    /**
    * Same method with EditText
    */
    int getPaddingEnd() = 46;
    /**
    * Same method with EditText
    */
    int getRight() = 47;
    /**
    * Same method with EditText
    */
    int getLeft() = 48;
    /**
    * Same method with EditText
    */
    int getTop() = 49;
    /**
    * Same method with EditText
    */
    int getBottom() = 50;
    /**
    * Same method with EditText
    */
    int getLineHeight() = 51;
    /**
    * Same method with EditText
    */
    int getLineCount() = 52;
    /**
    * Same method with EditText
    */
    int getBaseLine() = 53;
    // EditText Rect and Size, Position Getter }

    // { EditText layout Getter
    /**
    * Same method with EditText.getLayout()
    */
    int getParagraphDirection(int line) = 70;
    /**
    * Same method with EditText.getLayout()
    */
    float getPrimaryHorizontal(int offset) = 71;
    /**
    * Same method with EditText.getLayout()
    */
    float getLineMax(int i) = 72;
    /**
    * Same method with EditText.getLayout()
    */
    int getLineForOffset(int offset) = 73;
    /**
    * Same method with EditText.getLayout()
    */
    int getLineStart(int line) = 74;
    /**
    * Same method with EditText.getLayout()
    */
    int getLineEnd(int line) = 75;
    /**
    * Same method with EditText.getLayout()
    */
    int getLineTop(int line) = 76;
    /**
    * Same method with EditText.getLayout()
    */
    int getLineBottom(int line) = 77;
    /**
    * Same method with EditText.getLayout()
    */
    int getLineVisibleEnd(int line) = 78;
    /**
    * Same method with EditText.getLayout()
    */
    int getLineBaseline(int line) = 79;
    /**
    * Get line height by line index
    */
    int getLineHeightByIndex(int line) = 80;
    /**
    * Get line max inclues start padding of editText
    */
    int getLineMaxIncludePadding(int line) = 81;
    /**
    *  Same method with EditText.getLayout()
    */
    int getLineAscent(int line) = 82;
    /**
    *  Same method with EditText.getLayout()
    */
    int getLineDescent(int line) = 83;
    /**
    * Get text area by line index
    */
    Rect getTextAreaRect(int line) = 84;
    // EditText layout Getter }

    // { VI
    /**
    * Gets location of cursor for VI
    */
    PointF getCursorLocation(int selectionStart) = 90;
    // VI }

    // { EditText EditInfo Getter
    /**
    * Same method with EditText
    */
    String getPrivateImeOptions() = 100;
    /**
    * Same method with EditText
    */
    int getImeOptions() = 101;
    /**
    * Same method with EditText
    */
    int getInputType() = 102;
    // EditText EditInfo Getter }

    // { InputMethod
    /**
    * Same method with EditText
    * Should run on main looper
    */
    void onEditorAction(int actionCode) = 110;
    /**
    * Executes INPUT_METHOD_SERVICE sendAppPrivateCommand to send command to Keyboard
    *
    * @param action to send to keyboard
    * @param bundle to send to keyboard
    */
    void onAppPrivateCommand(String action, in Bundle bundle) = 111;
    /**
    * Hides keyboard forcely if it is showing for current input.
    */
    void semForceHideSoftInput() = 112;
    // InputMethod }

    // { Common Extra
    /**
    * Extra Command for future use
    *
    * @param action is for future use
    * @param bundle is for future use
    */
    void onExtraCommand(String action, inout Bundle bundle) = 900;
    // Common Extra }

    // { TextView
    /**
    * TextView Extra Command for future use.
    * Note: This Callback API receives the stylus writing Gesture recognized by service along with
    * gesture data bundle containing gesture coordinates, text to insert and alternate text to be
    * inserted in case gesture is not done over a valid text position in input.
    *
    * @param action is for future use
    * @param bundle is for future use
    */
    void onTextViewExtraCommand(String action, inout Bundle bundle) = 901;
    // TextView }

    /**
    * Direct writing service may be stopped to save memory when unused for a while. This method is
    * called to check if still hovering over writable fields to avoid stopping the service.
    *
    * @return true if the stylus handwriting hover icon is currently being shown, false otherwise.
    */
    boolean isHoverIconShowing() = 902;
}
