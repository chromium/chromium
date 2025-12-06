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

import java.util.Locale;

/**
 * Internal Mediator for the page zoom feature. Created by the |PageZoomIndicatorCoordinator|, and
 * should not be accessed outside the component.
 */
@NullMarked
class PageZoomIndicatorMediator {
    private final PropertyModel mModel;
    private final PageZoomManager mManager;
    private double mDefaultZoomFactor;

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
    void handleResetClicked() {
        mManager.setZoomLevel(mDefaultZoomFactor);
        updateZoomPercentageText(mDefaultZoomFactor);
        updateButtonStates(mDefaultZoomFactor);
    }

    @VisibleForTesting
    boolean isZoomLevelDefault() {
        return mManager.getZoomLevel() == mManager.getDefaultZoomLevel();
    }

    boolean isCurrentTabNull() {
        return mManager.isCurrentTabNull();
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
        popupWindow.setFocusable(true);
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
        long readableZoomLevel =
                Math.round(100 * PageZoomUtils.convertZoomFactorToZoomLevel(newZoomFactor));
        mModel.set(
                PageZoomProperties.ZOOM_PERCENT_TEXT,
                String.format(Locale.US, "%d%%", readableZoomLevel));
    }

    // Testing
    public PropertyModel getModelForTesting() {
        return mModel;
    }
}
