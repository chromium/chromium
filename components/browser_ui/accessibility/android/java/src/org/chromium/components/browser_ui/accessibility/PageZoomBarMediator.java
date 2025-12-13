// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.accessibility;

import static org.chromium.components.browser_ui.accessibility.PageZoomUtils.PAGE_ZOOM_MAXIMUM_BAR_VALUE;
import static org.chromium.components.browser_ui.accessibility.PageZoomUtils.convertZoomFactorToBarValue;
import static org.chromium.content_public.browser.HostZoomMap.AVAILABLE_ZOOM_FACTORS;
import static org.chromium.content_public.browser.HostZoomMap.setSystemFontScale;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Internal Mediator for the page zoom feature. Created by the |PageZoomCoordinator|, and should not
 * be accessed outside the component.
 */
@NullMarked
class PageZoomBarMediator {
    private final PropertyModel mModel;
    private final PageZoomManager mManager;
    private double mLatestZoomValue;
    private double mDefaultZoomFactor;

    PageZoomBarMediator(
            PropertyModel model, PageZoomManager manager, Callback<Void> userInteractionCallback) {
        mManager = manager;
        mModel = model;

        mModel.set(PageZoomProperties.DECREASE_ZOOM_CALLBACK, this::handleDecreaseClicked);
        mModel.set(PageZoomProperties.INCREASE_ZOOM_CALLBACK, this::handleIncreaseClicked);
        mModel.set(PageZoomProperties.RESET_ZOOM_CALLBACK, this::handleResetClicked);
        mModel.set(PageZoomProperties.BAR_VALUE_CHANGE_CALLBACK, this::handleBarValueChanged);
        mModel.set(PageZoomProperties.MAXIMUM_BAR_VALUE, PAGE_ZOOM_MAXIMUM_BAR_VALUE);
        mModel.set(PageZoomProperties.USER_INTERACTION_CALLBACK, userInteractionCallback);

        // Update the stored system font scale based on OS-level configuration. |this| will be
        // re-constructed after configuration changes, so this will be up-to-date for this session.
        setSystemFontScale(
                ContextUtils.getApplicationContext().getResources().getConfiguration().fontScale);
    }

    /** Initializes the mediator. */
    @Initializer
    protected void pushProperties() {
        // We must first fetch the current zoom factor for the given web contents.
        double currentZoomFactor = mManager.getZoomLevel();
        mDefaultZoomFactor = mManager.getDefaultZoomLevel();

        // The seekbar should start at the seek value that corresponds to this zoom factor.
        mModel.set(
                PageZoomProperties.CURRENT_BAR_VALUE,
                convertZoomFactorToBarValue(currentZoomFactor));

        mModel.set(PageZoomProperties.DEFAULT_ZOOM_FACTOR, mDefaultZoomFactor);

        updateButtonStates(currentZoomFactor);

        // Reset latest zoom value when initializing.
        mLatestZoomValue = 0.0;
    }

    /**
     * Returns the latest updated user selected zoom value during this session.
     *
     * @return double representing latest updated zoom value. Returns placeholder 0.0 if user did
     *     not select a zoom value during this session.
     */
    protected double latestZoomValue() {
        return mLatestZoomValue;
    }

    /** Logs UKM for the user changing the zoom level on the page from the slider. */
    protected void logZoomLevelUKM(double value) {
        assert mManager.getWebContents() != null : "WebContents is null";
        PageZoomMetrics.logZoomLevelUKM(mManager.getWebContents(), value);
    }

    @VisibleForTesting
    void handleDecreaseClicked() {
        handleIndexChanged(mManager.decrementZoomLevel());
    }

    @VisibleForTesting
    void handleIncreaseClicked() {
        handleIndexChanged(mManager.incrementZoomLevel());
    }

    void handleResetClicked() {
        // Reset as if the user moved the bar to the default zoom value
        handleBarValueChanged(PageZoomUtils.convertZoomFactorToBarValue(mDefaultZoomFactor));
    }

    void handleBarValueChanged(int newValue) {
        if (PageZoomUtils.shouldSnapBarValueToDefaultZoom(newValue, mDefaultZoomFactor)) {
            newValue = PageZoomUtils.convertZoomFactorToBarValue(mDefaultZoomFactor);
        }
        mModel.set(PageZoomProperties.CURRENT_BAR_VALUE, newValue);
        double zoomFactor = PageZoomUtils.convertBarValueToZoomFactor(newValue);
        mManager.setZoomLevel(zoomFactor);
        updateButtonStates(zoomFactor);
        mLatestZoomValue = PageZoomUtils.convertBarValueToZoomLevel(newValue);
    }

    private void handleIndexChanged(int index) {
        double zoomFactor = AVAILABLE_ZOOM_FACTORS[index];
        int barValue = PageZoomUtils.convertZoomFactorToBarValue(zoomFactor);
        mModel.set(PageZoomProperties.CURRENT_BAR_VALUE, barValue);
        mLatestZoomValue = PageZoomUtils.convertBarValueToZoomLevel(barValue);
        updateButtonStates(zoomFactor);
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
}
