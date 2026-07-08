// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.layouts.components;

import android.graphics.RectF;
import android.view.MotionEvent;
import android.view.View;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * {@link VirtualView} is the minimal interface that provides information for building accessibility
 * events.
 */
@NullMarked
public interface VirtualView {

    /**
     * The priority of the VirtualView. Higher values indicate higher priority when determining a11y
     * focus. This is used to handle conflicts when VirtualViews overlap.
     *
     * <p>If two overlapping views have the same priority, then the first one added to the list of
     * VirtualViews will be selected.
     *
     * <p>The {@link #INVALID} priority should not be used, except as a sentinel value when
     * iterating through a set of priorities.
     */
    @IntDef({
        VirtualViewPriority.INVALID,
        VirtualViewPriority.LOW,
        VirtualViewPriority.MEDIUM,
        VirtualViewPriority.HIGH
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface VirtualViewPriority {
        int INVALID = 0;
        int LOW = 1;
        int MEDIUM = 2;
        int HIGH = 3;
    }

    /**
     * @return A string with a description of the object for accessibility events.
     */
    String getAccessibilityDescription();

    /**
     * @param outTarget A rect that will be populated with the clickable area of the object in dp.
     */
    void getTouchTarget(RectF outTarget);

    /**
     * Returns the view's {@link VirtualViewPriority}. Higher values indicate higher priority when
     * determining a11y focus.
     */
    @VirtualViewPriority
    int getVirtualViewPriority();

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

    /** Notifies the view that it has received accessibility focus. */
    void onAccessibilityFocused();

    /**
     * Set keyboard focus state of {@link VirtualView} to {@param isFocused}.
     *
     * @param isFocused Whether this {@link VirtualView} is focused.
     */
    void setKeyboardFocused(boolean isFocused);

    /** Returns whether this {@link VirtualView} is keyboard focused. */
    boolean isKeyboardFocused();

    /**
     * Returns whether this view is enabled.
     *
     * <p>Note that this only reports the enabled state to accessibility services and does not
     * affect the clickability of the view.
     */
    default boolean isEnabled() {
        return true;
    }

    /** Returns the class name of this virtual view for accessibility. */
    default String getAccessibilityClassName() {
        return View.class.getName();
    }
}
