// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.util.motion;

import android.annotation.SuppressLint;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.util.MotionEventUtils;

/**
 * A {@link View.OnTouchListener} to detect a click from peripherals.
 *
 * <p>If a {@link MotionEvent} comes from a peripheral, this listener will consume it. Then, if the
 * event completes a click action, the {@link OnPeripheralClickRunnable} will be run.
 *
 * <p>If a {@link MotionEvent} doesn't come from a peripheral, this listener is a no-op. It won't
 * consume or interpret the event.
 */
@NullMarked
public final class OnPeripheralClickListener implements View.OnTouchListener {

    private final GestureDetector mGestureDetector;

    public OnPeripheralClickListener(
            View view, OnPeripheralClickRunnable onPeripheralClickRunnable) {
        mGestureDetector =
                new GestureDetector(
                        view.getContext(),
                        new GestureDetector.SimpleOnGestureListener() {

                            @Override
                            public boolean onSingleTapUp(MotionEvent e) {
                                onPeripheralClickRunnable.run(MotionEventInfo.fromMotionEvent(e));
                                return true;
                            }
                        });
    }

    @SuppressLint("ClickableViewAccessibility")
    @Override
    public boolean onTouch(View v, MotionEvent event) {
        if (!MotionEventUtils.isMouseEvent(event)) {
            return false;
        }

        mGestureDetector.onTouchEvent(event);
        return true;
    }

    public interface OnPeripheralClickRunnable {

        /**
         * Called when a peripheral click is detected.
         *
         * @param triggeringMotion {@link MotionEventInfo} that triggered the click.
         */
        void run(MotionEventInfo triggeringMotion);
    }
}
