// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pdf;

import android.content.Context;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

/** The top-level component responsible for the setup and lifecycle of the PDF Toolbar MVC stack. */
@NullMarked
public class PdfToolbarCoordinator implements View.OnClickListener {
    private final PropertyModel mModel;
    private final PdfToolbarActionsDelegate mDelegate;
    private final ArrayList<Float> mZoomLevels =
            new ArrayList<>(
                    List.of(
                            0.25f, 0.33f, 0.5f, 0.67f, 0.75f, 0.8f, 0.9f, 1.0f, 1.1f, 1.25f, 1.5f,
                            1.75f, 2.0f, 2.5f, 3.0f, 4.0f, 5.0f));

    public PdfToolbarCoordinator(
            Context context, View parentView, PdfToolbarActionsDelegate delegate) {
        mDelegate = delegate;
        PdfToolbar toolbar = parentView.findViewById(R.id.pdf_toolbar);
        // TODO(crbug.com/496180649): Only show the toolbar when the PDF is loaded via ViewStub.
        toolbar.setVisibility(View.VISIBLE);

        // Initialize the Model with all keys
        mModel =
                new PropertyModel.Builder(PdfToolbarProperties.ALL_KEYS)
                        .with(PdfToolbarProperties.ON_CLICK_LISTENER, this)
                        .with(PdfToolbarProperties.CURRENT_PAGE_NUMBER, 99)
                        .with(PdfToolbarProperties.TITLE, "This_is_a_pdf_title.pdf")
                        .with(PdfToolbarProperties.TOTAL_PAGE_COUNT, 100)
                        .with(PdfToolbarProperties.ZOOM_VALUE, "100%")
                        .build();

        // Set up the MCP to sync the Model and View
        PropertyModelChangeProcessor.create(mModel, toolbar, PdfToolbarViewBinder::bind);
    }

    @Override
    public void onClick(View view) {
        int actionId = view.getId();
        int currentPageNumber = mModel.get(PdfToolbarProperties.CURRENT_PAGE_NUMBER);
        int totalPageCount = mModel.get(PdfToolbarProperties.TOTAL_PAGE_COUNT);
        float currentZoomFactor =
                Float.parseFloat(mModel.get(PdfToolbarProperties.ZOOM_VALUE).replace("%", ""))
                        / 100f;

        if (actionId == R.id.page_increase_button && currentPageNumber < totalPageCount) {
            mDelegate.navigateToPage(currentPageNumber);
        } else if (actionId == R.id.page_decrease_button && currentPageNumber > 1) {
            mDelegate.navigateToPage(currentPageNumber - 2);
        } else if (actionId == R.id.zoom_increase_button) {
            mDelegate.changeZoomLevel(getNextZoomLevel(currentZoomFactor, true));
        } else if (actionId == R.id.zoom_decrease_button) {
            mDelegate.changeZoomLevel(getNextZoomLevel(currentZoomFactor, false));
        }
    }

    private float getNextZoomLevel(float currentZoomLevel, boolean increase) {
        int index = 0;

        // Find the first index where the zoom level is greater than or equal to current and move
        // to the next one if it exists.
        while (index < mZoomLevels.size() && mZoomLevels.get(index) <= currentZoomLevel) {
            index++;
        }

        if (increase) {
            // Return the next highest, or stay at the max if we're at the end
            return mZoomLevels.get(index);
        } else {
            //  If the current zoom level is in the list, decrease by 1. Otherwise, decrease by 2.
            int targetIndex = mZoomLevels.indexOf(currentZoomLevel) >= 0 ? index - 2 : index - 1;
            if (targetIndex < 0) return mZoomLevels.get(0);
            return mZoomLevels.get(targetIndex);
        }
    }

    /**
     * Called when the PDF document is successfully loaded.
     *
     * @param pageCount The total page count of the document.
     */
    public void onDocumentLoaded(int pageCount) {
        mModel.set(PdfToolbarProperties.TOTAL_PAGE_COUNT, pageCount);
    }

    /**
     * Called when the viewport changes on the PDF viewer.
     *
     * @param firstVisiblePage The first visible page.
     * @param zoomLevel The current zoom level.
     */
    public void onViewportChanged(int firstVisiblePage, float zoomLevel) {
        // Fetch absolute state from engine as the single source of truth.
        // Keep the model 1-indexed.
        mModel.set(PdfToolbarProperties.CURRENT_PAGE_NUMBER, firstVisiblePage + 1);
        mModel.set(
                PdfToolbarProperties.ZOOM_VALUE,
                String.format(Locale.ENGLISH, "%.0f%%", zoomLevel * 100));
        mModel.set(
                PdfToolbarProperties.ZOOM_DECREASE_BUTTON_ENABLED, zoomLevel > mZoomLevels.get(0));
        mModel.set(
                PdfToolbarProperties.ZOOM_INCREASE_BUTTON_ENABLED,
                zoomLevel < mZoomLevels.get(mZoomLevels.size() - 1));
    }
}
