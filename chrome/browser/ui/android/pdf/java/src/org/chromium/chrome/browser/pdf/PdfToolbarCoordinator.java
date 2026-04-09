// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pdf;

import android.content.Context;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** The top-level component responsible for the setup and lifecycle of the PDF Toolbar MVC stack. */
@NullMarked
public class PdfToolbarCoordinator implements View.OnClickListener {
    private final PropertyModel mModel;
    private final PdfToolbarActionsDelegate mDelegate;

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

        if (actionId == R.id.page_increase_button && currentPageNumber < totalPageCount) {
            mDelegate.navigateToPage(currentPageNumber);
        } else if (actionId == R.id.page_decrease_button && currentPageNumber > 1) {
            mDelegate.navigateToPage(currentPageNumber - 2);
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
     */
    public void onViewportChanged(int firstVisiblePage) {
        // Fetch absolute state from engine as the single source of truth.
        // Keep the model 1-indexed.
        mModel.set(PdfToolbarProperties.CURRENT_PAGE_NUMBER, firstVisiblePage + 1);
    }
}
