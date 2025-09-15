// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.accessibility;

import static org.chromium.content_public.browser.HostZoomMap.AVAILABLE_ZOOM_FACTORS;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.HostZoomMap;
import org.chromium.content_public.browser.WebContents;

/**
 * Manager for the page zoom features. This class is responsible for communicating with the
 * HostZoomMap class to set and get the zoom level of the current WebContents.
 */
@NullMarked
public class PageZoomManager {
    private final PageZoomManagerDelegate mDelegate;

    /**
     * @param delegate The delegate used to interact with the manager.
     */
    public PageZoomManager(PageZoomManagerDelegate delegate) {
        mDelegate = delegate;
    }

    /**
     * Decrements the zoom level of the current WebContents to the next closest zoom factor in the
     * cached available values.
     *
     * @return The index of the next closest zoom factor in the cached available values in the given
     *     direction from the current zoom factor.
     */
    @VisibleForTesting
    public int decrementZoomLevel() {
        // When decreasing zoom, "snap" to the greatest preset value that is less than the current.
        double currentZoomFactor = getZoomLevel();
        int index = PageZoomUtils.getNextIndex(true, currentZoomFactor);

        if (index >= 0) {
            snapToIndex(index);
        }
        return index;
    }

    /**
     * Increments the zoom level of the current WebContents to the next closest zoom factor in the
     * cached available values.
     *
     * @return The index of the next closest zoom factor in the cached available values in the given
     *     direction from the current zoom factor.
     */
    @VisibleForTesting
    public int incrementZoomLevel() {
        // When increasing zoom, "snap" to the smallest preset value that is more than the current.
        double currentZoomFactor = getZoomLevel();
        int index = PageZoomUtils.getNextIndex(false, currentZoomFactor);

        if (index <= AVAILABLE_ZOOM_FACTORS.length - 1) {
            snapToIndex(index);
        }
        return index;
    }

    /**
     * Returns the current zoom level of the current WebContents.
     *
     * @return The current zoom level of the current WebContents.
     */
    @VisibleForTesting
    public double getZoomLevel() {
        return HostZoomMap.getZoomLevel(mDelegate.getWebContents());
    }

    /**
     * Returns the default zoom level of the current Profile.
     *
     * @return The default zoom level of the current Profile.
     */
    @VisibleForTesting
    public double getDefaultZoomLevel() {
        return HostZoomMap.getDefaultZoomLevel(mDelegate.getBrowserContextHandle());
    }

    /**
     * Returns the WebContents of the current tab.
     *
     * @return The WebContents of the current tab.
     */
    @VisibleForTesting
    public @Nullable WebContents getWebContents() {
        return mDelegate.getWebContents();
    }

    /**
     * Sets the zoom level of the current WebContents to the given value.
     *
     * @param newZoomLevel The new zoom level to set the current WebContents to.
     */
    @VisibleForTesting
    public void setZoomLevel(double newZoomLevel) {
        HostZoomMap.setZoomLevel(mDelegate.getWebContents(), newZoomLevel);
    }

    // Snaps the zoom level of the current WebContents to the zoom factor at the given index in the
    // cached available values.
    @VisibleForTesting
    private void snapToIndex(int index) {
        double newZoomFactor = AVAILABLE_ZOOM_FACTORS[index];
        setZoomLevel(newZoomFactor);
    }
}
