// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.accessibility;

import static org.chromium.components.browser_ui.accessibility.PageZoomUtils.PAGE_ZOOM_MAXIMUM_SEEKBAR_VALUE;
import static org.chromium.components.browser_ui.accessibility.PageZoomUtils.convertZoomFactorToSeekBarValue;
import static org.chromium.content_public.browser.HostZoomMap.AVAILABLE_ZOOM_FACTORS;
import static org.chromium.content_public.browser.HostZoomMap.setSystemFontScale;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
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
    private double mLatestZoomValue;
    private double mDefaultZoomFactor;

    public PageZoomMediator(PropertyModel model) {
        mModel = model;

        mModel.set(PageZoomProperties.DECREASE_ZOOM_CALLBACK, this::handleDecreaseClicked);
        mModel.set(PageZoomProperties.INCREASE_ZOOM_CALLBACK, this::handleIncreaseClicked);
        mModel.set(PageZoomProperties.RESET_ZOOM_CALLBACK, this::handleResetClicked);
        mModel.set(PageZoomProperties.SEEKBAR_CHANGE_CALLBACK, this::handleSeekBarValueChanged);
        mModel.set(PageZoomProperties.MAXIMUM_SEEK_VALUE, PAGE_ZOOM_MAXIMUM_SEEKBAR_VALUE);

        // Update the stored system font scale based on OS-level configuration. |this| will be
        // re-constructed after configuration changes, so this will be up-to-date for this session.
        setSystemFontScale(
                ContextUtils.getApplicationContext().getResources().getConfiguration().fontScale);
    }

    /**
     * Set the web contents that should be controlled by this instance.
     * @param webContents   The WebContents this instance should control.
     */
    protected void setWebContents(WebContents webContents) {
        mWebContents = webContents;
        initialize();
    }

    /**
     * Returns the latest updated user selected zoom value during this session.
     * @return double representing latest updated zoom value. Returns placeholder 0.0 if user did
     *         not select a zoom value during this session.
     */
    protected double latestZoomValue() {
        return mLatestZoomValue;
    }

    /** Logs UKM for the user changing the zoom level on the page from the slider. */
    protected void logZoomLevelUKM(double value) {
        PageZoomMetrics.logZoomLevelUKM(mWebContents, value);
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    void handleDecreaseClicked(Void unused) {
        // When decreasing zoom, "snap" to the greatest preset value that is less than the current.
        double currentZoomFactor = getZoomLevel(mWebContents);
        int index = PageZoomUtils.getNextIndex(true, currentZoomFactor);

        if (index >= 0) {
            handleIndexChanged(index);
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    void handleIncreaseClicked(Void unused) {
        // When increasing zoom, "snap" to the smallest preset value that is more than the current.
        double currentZoomFactor = getZoomLevel(mWebContents);
        int index = PageZoomUtils.getNextIndex(false, currentZoomFactor);

        if (index <= AVAILABLE_ZOOM_FACTORS.length - 1) {
            handleIndexChanged(index);
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    void handleResetClicked(Void unused) {
        // Reset as if the user moved the seekbar to the default zoom value
        handleSeekBarValueChanged(
                PageZoomUtils.convertZoomFactorToSeekBarValue(mDefaultZoomFactor));
    }

    void handleSeekBarValueChanged(int newValue) {
        if (PageZoomUtils.shouldSnapSeekBarValueToDefaultZoom(newValue, mDefaultZoomFactor)) {
            newValue = PageZoomUtils.convertZoomFactorToSeekBarValue(mDefaultZoomFactor);
        }

        setZoomLevel(mWebContents, PageZoomUtils.convertSeekBarValueToZoomFactor(newValue));
        mModel.set(PageZoomProperties.CURRENT_SEEK_VALUE, newValue);
        updateButtonStates(PageZoomUtils.convertSeekBarValueToZoomFactor(newValue));
        mLatestZoomValue = PageZoomUtils.convertSeekBarValueToZoomLevel(newValue);
    }

    private void initialize() {
        // We must first fetch the current zoom factor for the given web contents.
        double currentZoomFactor = getZoomLevel(mWebContents);

        // The seekbar should start at the seek value that corresponds to this zoom factor.
        mModel.set(
                PageZoomProperties.CURRENT_SEEK_VALUE,
                convertZoomFactorToSeekBarValue(currentZoomFactor));

        mDefaultZoomFactor = mModel.get(PageZoomProperties.DEFAULT_ZOOM_FACTOR);

        updateButtonStates(currentZoomFactor);

        // Reset latest zoom value when initializing
        mLatestZoomValue = 0.0;
    }

    private void handleIndexChanged(int index) {
        double zoomFactor = AVAILABLE_ZOOM_FACTORS[index];
        int seekBarValue = PageZoomUtils.convertZoomFactorToSeekBarValue(zoomFactor);
        mModel.set(PageZoomProperties.CURRENT_SEEK_VALUE, seekBarValue);
        setZoomLevel(mWebContents, zoomFactor);
        updateButtonStates(zoomFactor);
        mLatestZoomValue = PageZoomUtils.convertSeekBarValueToZoomLevel(seekBarValue);
    }

    private void updateButtonStates(double newZoomFactor) {
        // If the new zoom factor is greater than the minimum zoom factor, enable decrease button.
        mModel.set(
                PageZoomProperties.DECREASE_ZOOM_ENABLED,
                newZoomFactor > AVAILABLE_ZOOM_FACTORS[0]);

        // If the new zoom factor is less than the maximum zoom factor, enable increase button.
        mModel.set(
                PageZoomProperties.INCREASE_ZOOM_ENABLED,
                newZoomFactor < AVAILABLE_ZOOM_FACTORS[AVAILABLE_ZOOM_FACTORS.length - 1]);
    }

    // Pass-through methods to HostZoomMap, which has static methods to call through JNI.
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    void setZoomLevel(@NonNull WebContents webContents, double newZoomLevel) {
        HostZoomMap.setZoomLevel(webContents, newZoomLevel);
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    double getZoomLevel(@NonNull WebContents webContents) {
        return HostZoomMap.getZoomLevel(webContents);
    }
}
