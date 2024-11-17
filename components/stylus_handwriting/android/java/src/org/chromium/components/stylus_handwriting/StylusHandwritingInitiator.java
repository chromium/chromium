// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.stylus_handwriting;

import static android.view.MotionEvent.TOOL_TYPE_STYLUS;

import android.os.Build.VERSION_CODES;
import android.view.MotionEvent;
import android.view.View;
import android.view.inputmethod.InputMethodManager;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.ui.base.ViewUtils;

/**
 * This class handles stylus touch events on a view to initiate handwriting input (requires Android
 * Tiramisu API level 33 or higher). It tracks stylus touch information and determines when to
 * initiate handwriting input based on pre-defined conditions like stylus type, movement slop, and
 * view writability.
 */
@RequiresApi(VERSION_CODES.TIRAMISU)
class StylusHandwritingInitiator {

    private static class StylusTouchData {
        public final float stylusDownX;
        public final float stylusDownY;
        public final int deviceId;

        // Constructor to update gathered Stylus information
        public StylusTouchData(int deviceId, float stylusDownX, float stylusDownY) {
            this.stylusDownX = stylusDownX;
            this.stylusDownY = stylusDownY;
            this.deviceId = deviceId;
        }
    }

    @Nullable private StylusTouchData mStylusTouchData;
    private final int mHandwritingSlopPx;
    private final InputMethodManager mInputMethodManager;

    StylusHandwritingInitiator(InputMethodManager inputMethodManager) {
        mInputMethodManager = inputMethodManager;
        // Handwriting slop for Stylus is 2dps
        mHandwritingSlopPx = ViewUtils.dpToPx(ContextUtils.getApplicationContext(), 2);
    }

    boolean onTouchEvent(@NonNull MotionEvent motionEvent, View view) {
        // Return false early if the feature is not enabled
        if (!isStylusHandwritingFeatureEnabled()) {
            return false;
        }
        // Return false early if the motion event is not coming from a Stylus.
        if (!isStylusEvent(motionEvent)) {
            clearStylusInfo();
            return false;
        }
        // Return false early if the motion event is coming from a second Stylus.
        int currentDeviceId = motionEvent.getDeviceId();
        if (mStylusTouchData != null && mStylusTouchData.deviceId != currentDeviceId) {
            return false;
        }
        int maskedAction = motionEvent.getActionMasked();
        switch (maskedAction) {
            case MotionEvent.ACTION_DOWN, MotionEvent.ACTION_POINTER_DOWN -> {
                updateStylusInfoOnDownEvent(motionEvent);
            }
            case MotionEvent.ACTION_MOVE -> {
                float current_x = motionEvent.getX();
                float current_y = motionEvent.getY();

                // Return early if the view isn't editable.
                if (!isViewWritable()) {
                    return false;
                }
                if (largerThanSlop(
                        current_x,
                        current_y,
                        mStylusTouchData.stylusDownX,
                        mStylusTouchData.stylusDownY,
                        mHandwritingSlopPx)) {
                    mInputMethodManager.startStylusHandwriting(view);
                    clearStylusInfo();
                    return true;
                }
                return false;
            }
            case MotionEvent.ACTION_CANCEL,
                    MotionEvent.ACTION_POINTER_UP,
                    MotionEvent.ACTION_UP -> {
                clearStylusInfo();
            }
        }
        return false;
    }

    private void updateStylusInfoOnDownEvent(MotionEvent motionEvent) {
        int actionIndex = motionEvent.getActionIndex();
        mStylusTouchData =
                new StylusTouchData(
                        motionEvent.getDeviceId(),
                        motionEvent.getX(actionIndex),
                        motionEvent.getY(actionIndex));
    }

    private void clearStylusInfo() {
        mStylusTouchData = null;
    }

    private boolean isStylusEvent(MotionEvent motionEvent) {
        return TOOL_TYPE_STYLUS == motionEvent.getToolType(motionEvent.getActionIndex());
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    boolean isViewWritable() {
        // TODO(crbug.com/317299999): Expose API to check whether event is over editable element.
        return true;
    }

    // Method to calculate if the difference between the starting point
    // and the current point of Stylus is bigger than the Slop
    private boolean largerThanSlop(
            float currentX, float currentY, float startingX, float startingY, int handwritingSlop) {
        float distanceX = currentX - startingX;
        float distanceY = currentY - startingY;
        return (distanceX * distanceX) + (distanceY * distanceY)
                >= handwritingSlop * handwritingSlop;
    }

    // Method to check if the corresponding feature flag is enabled
    boolean isStylusHandwritingFeatureEnabled() {
        return StylusHandwritingFeatureMap.isEnabled(
                StylusHandwritingFeatureMap.USE_HANDWRITING_INITIATOR);
    }
}
