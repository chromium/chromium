// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.layouts.components;

import android.graphics.RectF;
import android.view.MotionEvent;

import org.chromium.build.annotations.NullMarked;

/**
 * {@link VirtualView} is the minimal interface that provides information for building accessibility
 * events.
 */
@NullMarked
public interface VirtualView {
    /**
     * @return A string with a description of the object for accessibility events.
     */
    String getAccessibilityDescription();

    /**
     * @param outTarget A rect that will be populated with the clickable area of the object in dp.
     */
    void getTouchTarget(RectF outTarget);

    /**
     * @param x The x offset of the click in dp.
     * @param y The y offset of the click in dp.
     * @return Whether or not that click occurred inside of the button + slop area.
     */
    boolean checkClickedOrHovered(float x, float y);

    /**
     * @return Whether there is a click action associated with this virtual view.
     */
    default boolean hasClickAction() {
        return true;
    }

    /**
     * @return Whether there is a long click action associated with this virtual view.
     */
    default boolean hasLongClickAction() {
        return true;
    }

    /**
     * Notifies the view to handle the click action.
     *
     * @param time The time of the click action.
     * @param motionEventButtonState {@link MotionEvent#getButtonState()} at the moment of the click
     *     if the click is detected via motion events; otherwise, this parameter is {@link
     *     org.chromium.ui.util.MotionEventUtils#MOTION_EVENT_BUTTON_NONE}.
     * @param modifiers State of all Meta/Modifier keys that are pressed.
     */
    void handleClick(long time, int motionEventButtonState, int modifiers);

    /**
     * Set keyboard focus state of {@link VirtualView} to {@param isFocused}.
     *
     * @param isFocused Whether this {@link VirtualView} is focused.
     */
    void setKeyboardFocused(boolean isFocused);

    /** Returns whether this {@link VirtualView} is keyboard focused. */
    boolean isKeyboardFocused();
}
