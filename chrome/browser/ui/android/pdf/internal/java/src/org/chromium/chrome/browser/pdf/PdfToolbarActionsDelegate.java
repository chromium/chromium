// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pdf;

import org.chromium.build.annotations.NullMarked;

/** Interface to handle actions from the PDF toolbar. */
@NullMarked
public interface PdfToolbarActionsDelegate {
    /**
     * Navigates to the specified page.
     *
     * @param pageIndex The 0-based index of the page to navigate to.
     */
    void navigateToPage(int pageIndex);

    /**
     * Changes the zoom level of the PDF viewer.
     *
     * @param zoomLevel The new zoom level.
     */
    void changeZoomLevel(float zoomLevel);

    /**
     * Toggles between "fit to page height" and "fit to page width" modes.
     *
     * @param fitToPageHeight Whether to fit to page height or fit to page width.
     * @param pageIndex The 0-based index of the page to navigate to.
     */
    void toggleFitToPage(boolean fitToPageHeight, int pageIndex);

    /**
     * Toggles whether the PDF viewer should display two pages per row.
     *
     * @param twoPagesPerRowEnabled Whether the PDF viewer should display two pages per row.
     * @param zoomLevel The current zoom level. This is used to maintain the current zoom level when
     *     toggling between one and two pages per row.
     * @param currentPageIndex The current page index. This is used to maintain the current page
     *     position when toggling between one and two pages per row.
     */
    void toggleTwoPagesPerRow(boolean twoPagesPerRowEnabled, float zoomLevel, int currentPageIndex);

    /** Downloads the PDF document. */
    void download();

    /** Rotates the PDF document. */
    void rotate();
}
