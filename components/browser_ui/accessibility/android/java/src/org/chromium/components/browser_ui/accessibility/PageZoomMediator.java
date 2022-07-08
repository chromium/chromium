// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.accessibility;

import static org.chromium.components.browser_ui.accessibility.PageZoomUtils.AVAILABLE_ZOOM_FACTORS;
import static org.chromium.components.browser_ui.accessibility.PageZoomUtils.PAGE_ZOOM_MAXIMUM_SEEKBAR_VALUE;
import static org.chromium.components.browser_ui.accessibility.PageZoomUtils.convertZoomFactorToSeekBarValue;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.MathUtils;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.HostZoomMap;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Internal Mediator for the page zoom feature. Created by the |PageZoomCoordinator|, and should
 * not be accessed outside the component.
 */
public class PageZoomMediator {
    private final PropertyModel mModel;
    private WebContents mWebContents;

    public PageZoomMediator(PropertyModel model) {
        mModel = model;

        mModel.set(PageZoomProperties.DECREASE_ZOOM_CALLBACK, this::handleDecreaseClicked);
        mModel.set(PageZoomProperties.INCREASE_ZOOM_CALLBACK, this::handleIncreaseClicked);
        mModel.set(PageZoomProperties.SEEKBAR_CHANGE_CALLBACK, this::handleSeekBarValueChanged);
        mModel.set(PageZoomProperties.MAXIMUM_SEEK_VALUE, PAGE_ZOOM_MAXIMUM_SEEKBAR_VALUE);
    }

    /**
     * Returns whether the AppMenu item for Zoom should be displayed. It will be displayed if
     * any of the following conditions are met:
     *
     *    - User has enabled the "Show Page Zoom" setting in Chrome Accessibility Settings
     *    - User has set a default zoom other than 100% in Chrome Accessibility Settings
     *    - User has changed the Android OS Font Size setting
     *
     * @return boolean
     */
    protected static boolean shouldShowMenuItem() {
        // Never show the menu item if the content feature is disabled.
        if (!ContentFeatureList.isEnabled(ContentFeatureList.ACCESSIBILITY_PAGE_ZOOM)) {
            return false;
        }

        // Always show the menu item if the user has set this in Accessibility Settings.
        if (PageZoomUtils.shouldAlwaysShowZoomMenuItem()) {
            return true;
        }

        // The default (float) |fontScale| is 1, the default page zoom is 1.
        boolean defaultSystemFontSize = MathUtils.areFloatsEqual(
                ContextUtils.getApplicationContext().getResources().getConfiguration().fontScale,
                1f);

        // TODO(mschillaci): Replace with a delegate call, cannot depend directly on Profile.
        boolean defaultDefaultPageZoom = true;

        return !defaultSystemFontSize || !defaultDefaultPageZoom;
    }

    /**
     * Set the web contents that should be controlled by this instance.
     * @param webContents   The WebContents this instance should control.
     */
    protected void setWebContents(WebContents webContents) {
        mWebContents = webContents;
        initialize();
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    void handleDecreaseClicked(Void unused) {
        // When decreasing zoom, "snap" to the greatest preset value that is less than the current.
        double currentZoomFactor = getZoomLevel(mWebContents);
        int index = PageZoomUtils.getNextIndex(true, currentZoomFactor);

        if (index >= 0) {
            mModel.set(PageZoomProperties.CURRENT_SEEK_VALUE,
                    PageZoomUtils.convertZoomFactorToSeekBarValue(AVAILABLE_ZOOM_FACTORS[index]));
            setZoomLevel(mWebContents, AVAILABLE_ZOOM_FACTORS[index]);
            updateButtonStates(AVAILABLE_ZOOM_FACTORS[index]);
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    void handleIncreaseClicked(Void unused) {
        // When increasing zoom, "snap" to the smallest preset value that is more than the current.
        double currentZoomFactor = getZoomLevel(mWebContents);
        int index = PageZoomUtils.getNextIndex(false, currentZoomFactor);

        if (index <= AVAILABLE_ZOOM_FACTORS.length - 1) {
            mModel.set(PageZoomProperties.CURRENT_SEEK_VALUE,
                    PageZoomUtils.convertZoomFactorToSeekBarValue(AVAILABLE_ZOOM_FACTORS[index]));
            setZoomLevel(mWebContents, AVAILABLE_ZOOM_FACTORS[index]);
            updateButtonStates(AVAILABLE_ZOOM_FACTORS[index]);
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    void handleSeekBarValueChanged(int newValue) {
        setZoomLevel(mWebContents, PageZoomUtils.convertSeekBarValueToZoomFactor(newValue));
        mModel.set(PageZoomProperties.CURRENT_SEEK_VALUE, newValue);
        updateButtonStates(PageZoomUtils.convertSeekBarValueToZoomFactor(newValue));
    }

    private void initialize() {
        // We must first fetch the current zoom factor for the given web contents.
        double currentZoomFactor = getZoomLevel(mWebContents);

        // The seekbar should start at the seek value that corresponds to this zoom factor.
        mModel.set(PageZoomProperties.CURRENT_SEEK_VALUE,
                convertZoomFactorToSeekBarValue(currentZoomFactor));

        updateButtonStates(currentZoomFactor);
    }

    private void updateButtonStates(double newZoomFactor) {
        // If the new zoom factor is greater than the minimum zoom factor, enable decrease button.
        mModel.set(PageZoomProperties.DECREASE_ZOOM_ENABLED,
                newZoomFactor > AVAILABLE_ZOOM_FACTORS[0]);

        // If the new zoom factor is less than the maximum zoom factor, enable increase button.
        mModel.set(PageZoomProperties.INCREASE_ZOOM_ENABLED,
                newZoomFactor < AVAILABLE_ZOOM_FACTORS[AVAILABLE_ZOOM_FACTORS.length - 1]);
    }

    // Pass-through methods to HostZoomMap, which has static methods to call through JNI.
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    void setZoomLevel(@NonNull WebContents webContents, double newZoomLevel) {
        HostZoomMap.setZoomLevel(webContents, newZoomLevel);
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    double getZoomLevel(@NonNull WebContents webContents) {
        return PageZoomUtils.roundTwoDecimalPlaces(HostZoomMap.getZoomLevel(webContents));
    }
}
