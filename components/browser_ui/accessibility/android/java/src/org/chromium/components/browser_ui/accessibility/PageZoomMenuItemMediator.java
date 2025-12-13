// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.accessibility;

import static org.chromium.content_public.browser.HostZoomMap.AVAILABLE_ZOOM_FACTORS;
import static org.chromium.content_public.browser.HostZoomMap.setSystemFontScale;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Locale;

/**
 * Internal Mediator for the page zoom feature. Created by the |PageZoomMenuItemCoordinator|, and
 * should not be accessed outside the component.
 */
@NullMarked
class PageZoomMenuItemMediator {
    private final PropertyModel mModel;
    private final PageZoomManager mManager;
    private double mDefaultZoomFactor;

    PageZoomMenuItemMediator(PropertyModel model, PageZoomManager manager) {
        mManager = manager;
        mModel = model;

        mModel.set(PageZoomProperties.DECREASE_ZOOM_CALLBACK, this::handleDecreaseClicked);
        mModel.set(PageZoomProperties.INCREASE_ZOOM_CALLBACK, this::handleIncreaseClicked);
        mModel.set(PageZoomProperties.IMMERIVE_MODE_CALLBACK, this::handleImmersiveModeClicked);

        // Update the stored system font scale based on OS-level configuration. `this` will be
        // reconstructed after configuration changes, so this will be up-to-date for this session.
        setSystemFontScale(
                ContextUtils.getApplicationContext().getResources().getConfiguration().fontScale);

        pushProperties();
    }

    /** Initializes the mediator. */
    @Initializer
    protected void pushProperties() {
        // We must first fetch the current zoom factor for the given web contents.
        double currentZoomFactor = mManager.getZoomLevel();
        mDefaultZoomFactor = mManager.getDefaultZoomLevel();
        updateZoomPercentageText(currentZoomFactor);

        mModel.set(PageZoomProperties.DEFAULT_ZOOM_FACTOR, mDefaultZoomFactor);

        updateButtonStates(currentZoomFactor);
    }

    @VisibleForTesting
    void handleDecreaseClicked() {
        handleIndexChanged(mManager.decrementZoomLevel());
    }

    @VisibleForTesting
    void handleIncreaseClicked() {
        handleIndexChanged(mManager.incrementZoomLevel());
    }

    @VisibleForTesting
    void handleImmersiveModeClicked() {
        mManager.enterImmersiveMode();
        PageZoomUma.logImmersiveModeClicked();
    }

    private void handleIndexChanged(int index) {
        double zoomFactor = AVAILABLE_ZOOM_FACTORS[index];
        updateZoomPercentageText(zoomFactor);
        updateButtonStates(zoomFactor);
        PageZoomUma.logLffAppMenuUsageHistogram();
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

    private void updateZoomPercentageText(double newZoomFactor) {
        long readableZoomLevel = PageZoomUtils.getReadableZoomLevel(newZoomFactor);
        mModel.set(
                PageZoomProperties.ZOOM_PERCENT_TEXT,
                String.format(Locale.US, "%d%%", readableZoomLevel));
    }
}
