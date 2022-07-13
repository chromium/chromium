// Copyright 2022 The Chromium Authors. All rights reserved.
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
    const int VERSION = 1;

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
    // Bound EditText }

    // { EditText Text and Selection Setter
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
    int getOffsetForPosition(float x, float y) = 32;
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

    // { Common Extra
    /**
    * Extra Command for furture use
    *
    * @param action is for furture use
    * @param bundle is for furture use
    */
    void onExtraCommand(String action, inout Bundle bundle) = 900;
    // Common Extra }
}
