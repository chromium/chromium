// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.accessibility;

import static org.chromium.content_public.browser.HostZoomMap.AVAILABLE_ZOOM_FACTORS;
import static org.chromium.content_public.browser.HostZoomMap.setSystemFontScale;

import android.view.View;
import android.view.ViewGroup;
import android.widget.PopupWindow;
import android.widget.PopupWindow.OnDismissListener;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Internal Mediator for the page zoom feature. Created by the |PageZoomIndicatorCoordinator|, and
 * should not be accessed outside the component.
 */
@NullMarked
class PageZoomIndicatorMediator {
    private final PropertyModel mModel;
    private final PageZoomManager mManager;

    PageZoomIndicatorMediator(PageZoomManager manager) {
        mManager = manager;
        mModel =
                new PropertyModel.Builder(PageZoomProperties.ALL_KEYS_FOR_INDICATOR)
                        .with(
                                PageZoomProperties.DECREASE_ZOOM_CALLBACK,
                                this::handleDecreaseClicked)
                        .with(
                                PageZoomProperties.INCREASE_ZOOM_CALLBACK,
                                this::handleIncreaseClicked)
                        .with(PageZoomProperties.RESET_ZOOM_CALLBACK, this::handleResetClicked)
                        .build();

        // Update the stored system font scale based on OS-level configuration. |this| will be
        // re-constructed after configuration changes, so this will be up-to-date for this session.
        setSystemFontScale(
                ContextUtils.getApplicationContext().getResources().getConfiguration().fontScale);
    }

    /** Sets the initial state of the model. */
    void pushProperties() {
        updateZoomPercentage();
    }

    /** Updates the zoom percentage text and button states for the current zoom factor. */
    void updateZoomPercentage() {
        double currentZoomFactor = mManager.getZoomLevel();
        updateZoomPercentageText(currentZoomFactor);
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
    void handleResetClicked() {
        mManager.resetZoomLevel();
        updateZoomPercentage();
    }

    PopupWindow buildPopupWindow(View view, OnDismissListener onDismissListener) {
        PropertyModelChangeProcessor.create(mModel, view, PageZoomIndicatorViewBinder::bind);
        PopupWindow popupWindow =
                new PopupWindow(
                        view,
                        ViewGroup.LayoutParams.WRAP_CONTENT,
                        ViewGroup.LayoutParams.WRAP_CONTENT);
        popupWindow.setElevation(
                view.getContext().getResources().getDimension(R.dimen.dropdown_elevation));
        popupWindow.setFocusable(false);
        popupWindow.setOutsideTouchable(true);
        popupWindow.setOnDismissListener(onDismissListener);

        return popupWindow;
    }

    void showPopupWindow(View anchorView, PopupWindow popupWindow) {
        // Measure the content view to get its width.
        int popupWidth =
                anchorView
                        .getContext()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.page_zoom_indicator_popup_width);

        int offset =
                anchorView
                        .getContext()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.page_zoom_indicator_popup_dropdown_offset);
        popupWindow.showAsDropDown(anchorView, anchorView.getWidth() - popupWidth + offset, 0);
    }

    private void handleIndexChanged(int index) {
        double zoomFactor = AVAILABLE_ZOOM_FACTORS[index];
        updateZoomPercentageText(zoomFactor);
        updateButtonStates(zoomFactor);
    }

    private void updateButtonStates(double newZoomFactor) {
        mModel.set(
                PageZoomProperties.DECREASE_ZOOM_ENABLED,
                PageZoomUtils.canDecreaseZoom(newZoomFactor));
        mModel.set(
                PageZoomProperties.INCREASE_ZOOM_ENABLED,
                PageZoomUtils.canIncreaseZoom(newZoomFactor));
    }

    private void updateZoomPercentageText(double newZoomFactor) {
        mModel.set(
                PageZoomProperties.ZOOM_PERCENT_TEXT,
                PageZoomUtils.formatZoomPercentage(newZoomFactor));
    }

    // Testing
    PropertyModel getModelForTesting() {
        return mModel;
    }
}
