// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.accessibility;

import static org.chromium.components.browser_ui.accessibility.PageZoomUtils.PAGE_ZOOM_MAXIMUM_SEEKBAR_VALUE;
import static org.chromium.components.browser_ui.accessibility.PageZoomUtils.convertZoomFactorToSeekBarValue;
import static org.chromium.content_public.browser.HostZoomMap.AVAILABLE_ZOOM_FACTORS;
import static org.chromium.content_public.browser.HostZoomMap.SYSTEM_FONT_SCALE;

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
    private double mLatestZoomValue;

    public PageZoomMediator(PropertyModel model) {
        mModel = model;

        mModel.set(PageZoomProperties.DECREASE_ZOOM_CALLBACK, this::handleDecreaseClicked);
        mModel.set(PageZoomProperties.INCREASE_ZOOM_CALLBACK, this::handleIncreaseClicked);
        mModel.set(PageZoomProperties.SEEKBAR_CHANGE_CALLBACK, this::handleSeekBarValueChanged);
        mModel.set(PageZoomProperties.MAXIMUM_SEEK_VALUE, PAGE_ZOOM_MAXIMUM_SEEKBAR_VALUE);

        // Update the stored system font scale based on OS-level configuration. |this| will be
        // re-constructed after configuration changes, so this will be up-to-date for this session.
        SYSTEM_FONT_SCALE =
                ContextUtils.getApplicationContext().getResources().getConfiguration().fontScale;
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
            PageZoomUma.logAppMenuEnabledStateHistogram(
                    PageZoomUma.AccessibilityPageZoomAppMenuEnabledState.USER_ENABLED);
            return true;
        }

        // The default (float) |fontScale| is 1, the default page zoom is 1.
        // If the user has a system font scale other than the default, always show the menu item.
        boolean isUsingDefaultSystemFontScale = MathUtils.areFloatsEqual(SYSTEM_FONT_SCALE, 1f);
        if (!isUsingDefaultSystemFontScale) {
            PageZoomUma.logAppMenuEnabledStateHistogram(
                    PageZoomUma.AccessibilityPageZoomAppMenuEnabledState.OS_ENABLED);
            return true;
        }

        // TODO(mschillaci): Decide whether to additionally enable app menu item depending on
        // default page zoom. If yes, then replace with a delegate call, cannot depend directly on
        // Profile.
        PageZoomUma.logAppMenuEnabledStateHistogram(
                PageZoomUma.AccessibilityPageZoomAppMenuEnabledState.NOT_ENABLED);
        return false;
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

    /**
     * Logs UKM for the user changing the zoom level on the page from the slider.
     */
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
    void handleSeekBarValueChanged(int newValue) {
        setZoomLevel(mWebContents, PageZoomUtils.convertSeekBarValueToZoomFactor(newValue));
        mModel.set(PageZoomProperties.CURRENT_SEEK_VALUE, newValue);
        updateButtonStates(PageZoomUtils.convertSeekBarValueToZoomFactor(newValue));
        mLatestZoomValue = PageZoomUtils.convertSeekBarValueToZoomLevel(newValue);
    }

    private void initialize() {
        // We must first fetch the current zoom factor for the given web contents.
        double currentZoomFactor = getZoomLevel(mWebContents);

        // The seekbar should start at the seek value that corresponds to this zoom factor.
        mModel.set(PageZoomProperties.CURRENT_SEEK_VALUE,
                convertZoomFactorToSeekBarValue(currentZoomFactor));

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
        return HostZoomMap.getZoomLevel(webContents);
    }
}
