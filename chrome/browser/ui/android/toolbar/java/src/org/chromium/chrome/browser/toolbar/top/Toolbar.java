// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import android.graphics.Rect;
import android.view.View;
import android.widget.ProgressBar;

import org.chromium.chrome.browser.toolbar.ToolbarProgressBar;

/**
 * An interface for outside packages to interact with the top toolbar. Other than for testing
 * purposes this interface should be used rather than {@link TopToolbarCoordinator} or
 * {@link ToolbarLayout} and extending classes.
 */
public interface Toolbar {
    /**
     * Calculates the {@link Rect} that represents the content area of the location bar.  This
     * rect will be relative to the toolbar.
     * @param outRect The Rect that represents the content area of the location bar.
     */
    void getLocationBarContentRect(Rect outRect);

    /**
     * @return Whether any swipe gestures should be ignored for the current Toolbar state.
     */
    boolean shouldIgnoreSwipeGesture();

    /**
     * Calculate the relative position wrt to the given container view.
     * @param containerView The container view to be used.
     * @param position The position array to be used for returning the calculated position.
     */
    void getPositionRelativeToContainer(View containerView, int[] position);

    /**
     * Get the height of the toolbar in px.
     * @return The height of the toolbar.
     */
    int getHeight();

    /**
     * Sets whether or not the toolbar should draw as if it's being captured for a snapshot
     * texture.  In this mode it will only draw the toolbar in it's normal state (no TabSwitcher
     * or animations).
     * @param textureMode Whether or not to be in texture capture mode.
     */
    void setTextureCaptureMode(boolean textureMode);

    /**
     * @return Whether a dirty check for invalidation makes sense at this time.
     */
    CaptureReadinessResult isReadyForTextureCapture();

    /**
     * Sets whether or not the toolbar should force itself to draw for a texture capture regardless
     * of other criteria used in isReadyForTextureCapture(). A texture capture will only be forced
     * if the toolbar drawables tint is changing.
     *
     * @param forceTextureCapture Whether the toolbar should force itself to draw.
     * @return True if a texture capture will be forced on the next draw.
     */
    boolean setForceTextureCapture(boolean forceTextureCapture);

    /**
     * Returns the height of the tab strip, iff the toolbar has one. Returns 0 for toolbars that do
     * not have a tabstrip.
     * @return height of the tab strip in px.
     */
    int getTabStripHeight();

    /**
     * Disable the menu button. This removes the view from the hierarchy and nulls the related
     * instance vars.
     */
    void disableMenuButton();

    /**
     * @return The {@link ProgressBar} this layout uses.
     */
    ToolbarProgressBar getProgressBar();

    /**
     * @return The primary color to use for the background drawable.
     */
    int getPrimaryColor();

    /**
     * Updates the visibility of the reload button.
     * @param isReloading Whether or not the page is loading.
     */
    void updateReloadButtonVisibility(boolean isReloading);

    /**
     * Updates the visibility of the toolbar hairline.
     * @param isVisible whether or not the hairline should be visible.
     * */
    void setBrowsingModeHairlineVisibility(boolean isVisible);

    /**
     * Returns whether the ToolbarLayout is visible. The ToolbarLayout might be gone when the Start
     * surface's toolbar is showing.
     */
    boolean isBrowsingModeToolbarVisible();
}
