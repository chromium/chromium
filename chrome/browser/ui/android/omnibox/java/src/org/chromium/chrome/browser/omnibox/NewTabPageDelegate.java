// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.graphics.Point;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.view.MotionEvent;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;

/**
 * Delegate that provides the toolbar with the info of the NTP for the current tab.
 * TODO(crbug.com/40148706): Consider moving this out of toolbar/ into its own target for omnibox as
 * well.
 */
public interface NewTabPageDelegate {
    /**
     * @return {@code true} if the current tab was showing NewTabPage.
     */
    default boolean wasShowingNtp() {
        return false;
    }

    /**
     * @return {@code true} if the NewTabPage is currently visible.
     */
    default boolean isCurrentlyVisible() {
        return false;
    }

    /**
     * @return Whether the location bar is shown in the NTP.
     */
    default boolean isLocationBarShown() {
        return false;
    }

    /**
     * @return {@code true} if we're transitioning away from showing the location bar.
     */
    default boolean transitioningAwayFromLocationBar() {
        return false;
    }

    /**
     * Returns whether the first layout pass has happened or not. When false, this often means there
     * is some animation playing for creating the tab itself. During this time the NTP will not be
     * able to control any drawing, and the toolbar will still be responsible for drawing itself.
     */
    default boolean hasCompletedFirstLayout() {
        return false;
    }

    /**
     * Set the listener for NTP to handle the scroll event.
     *
     * @param scrollCallback Callback to be invoked when the event occurs.
     */
    default void setSearchBoxScrollListener(@Nullable Callback<Float> scrollCallback) {}

    /**
     * Get the bounds of the search box in relation to the top level NewTabPage view.
     *
     * @param bounds The current drawing location of the search box.
     * @param translation The translation applied to the search box by the parent view hierarchy up
     *     to the NewTabPage view.
     */
    default void getSearchBoxBounds(Rect bounds, Point translation) {}

    /**
     * Updates the opacity of the search box when scrolling.
     *
     * @param alpha opacity (alpha) value to use.
     */
    default void setSearchBoxAlpha(float alpha) {}

    /**
     * Updates the opacity of the search provider logo when scrolling.
     *
     * @param alpha opacity (alpha) value to use.
     */
    default void setSearchProviderLogoAlpha(float alpha) {}

    /**
     * Set the search box background drawable.
     *
     * @param drawable The search box background.
     */
    default void setSearchBoxBackground(Drawable drawable) {}

    /**
     * Specifies the percentage the URL is focused during an animation. 1.0 specifies that the URL
     * bar has focus and has completed the focus animation. 0 is when the URL bar is does not have
     * any focus.
     *
     * @param fraction The percentage of the URL bar focus animation.
     */
    default void setUrlFocusChangeAnimationPercent(float fraction) {}

    /**
     * Pass the motion event to NewTabPage object.
     *
     * @see {@link View#dispatchTouchEvent(MotionEvent)}
     */
    default boolean dispatchTouchEvent(MotionEvent ev) {
        return false;
    }

    /** Empty implementation of NewTabDelegate. Used for a default before initialization. */
    public static final NewTabPageDelegate EMPTY = new NewTabPageDelegate() {};
}
