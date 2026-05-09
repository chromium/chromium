// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pdf;

import android.view.View;
import android.view.ViewStub;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.List;

/** The top-level component responsible for the setup and lifecycle of the PDF Toolbar MVC stack. */
@NullMarked
public class PdfToolbarCoordinator implements View.OnClickListener {
    private final PropertyModel mModel;
    private final PdfToolbarActionsDelegate mDelegate;
    private final PropertyModelChangeProcessor<PropertyModel, PdfToolbar, PropertyKey>
            mPropertyModelChangeProcessor;
    private final List<Float> mZoomLevels =
            List.of(
                    0.25f, 0.33f, 0.5f, 0.67f, 0.75f, 0.8f, 0.9f, 1.0f, 1.1f, 1.25f, 1.5f, 1.75f,
                    2.0f, 2.5f, 3.0f, 4.0f, 5.0f);

    public PdfToolbarCoordinator(View parentView, PdfToolbarActionsDelegate delegate) {
        mDelegate = delegate;
        PdfToolbar toolbar = parentView.findViewById(R.id.pdf_toolbar);
        if (toolbar == null) {
            ViewStub stub = parentView.findViewById(R.id.pdf_toolbar_stub);
            assert stub != null;
            toolbar = (PdfToolbar) stub.inflate();
        }
        toolbar.setVisibility(View.VISIBLE);

        // TODO(crbug.com/507061296): Remove hardcoded values after the PDF is loaded.
        mModel =
                new PropertyModel.Builder(PdfToolbarProperties.ALL_KEYS)
                        .with(PdfToolbarProperties.ON_CLICK_LISTENER, this)
                        .build();

        // Set up the MCP to sync the Model and View
        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(mModel, toolbar, PdfToolbarViewBinder::bind);
    }

    @Override
    public void onClick(View view) {
        int actionId = view.getId();
        int currentPageNumber = mModel.get(PdfToolbarProperties.CURRENT_PAGE_NUMBER);
        int totalPageCount = mModel.get(PdfToolbarProperties.TOTAL_PAGE_COUNT);
        float currentZoomFactor = mModel.get(PdfToolbarProperties.ZOOM_LEVEL);

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
            // If the current zoom level is in the list, decrease by 1. Otherwise, decrease by 2.
            int targetIndex = mZoomLevels.contains(currentZoomLevel) ? index - 2 : index - 1;
            if (targetIndex < 0) targetIndex = 0;
            return mZoomLevels.get(targetIndex);
        }
    }

    /**
     * Called when the PDF document is successfully loaded.
     *
     * @param pageCount The total page count of the document.
     * @param title The title of the document.
     */
    public void onDocumentLoaded(int pageCount, String title) {
        mModel.set(PdfToolbarProperties.TOTAL_PAGE_COUNT, pageCount);
        mModel.set(PdfToolbarProperties.TITLE, title);
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
        mModel.set(PdfToolbarProperties.ZOOM_LEVEL, zoomLevel);
        mModel.set(
                PdfToolbarProperties.ZOOM_DECREASE_BUTTON_ENABLED, zoomLevel > mZoomLevels.get(0));
        mModel.set(
                PdfToolbarProperties.ZOOM_INCREASE_BUTTON_ENABLED,
                zoomLevel < mZoomLevels.get(mZoomLevels.size() - 1));
    }

    /** Destroys the coordinator and releases references held by the change processor. */
    public void destroy() {
        mPropertyModelChangeProcessor.destroy();
    }
}
