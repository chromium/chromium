// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.layouts.components;

import android.graphics.RectF;

/**
 * {@link VirtualView} is the minimal interface that provides information for
 * building accessibility events.
 */
public interface VirtualView {
    /**
     * @return A string with a description of the object for accessibility events.
     */
    String getAccessibilityDescription();

    /**
     * @param A rect that will be populated with the clickable area of the object in dp.
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
     */
    void handleClick(long time);
}
