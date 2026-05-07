// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.pdf;

import android.view.View;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** View binder for the PDF toolbar. */
@NullMarked
class PdfToolbarViewBinder {
    /**
     * Binds the model to the view.
     *
     * @param model The PropertyModel.
     * @param view The PdfToolbar view.
     * @param key The PropertyKey that changed.
     */
    static void bind(PropertyModel model, PdfToolbar view, PropertyKey key) {
        if (PdfToolbarProperties.CURRENT_PAGE_NUMBER == key) {
            TextView currentPage = view.findViewById(R.id.current_page);
            currentPage.setText(
                    String.valueOf(model.get(PdfToolbarProperties.CURRENT_PAGE_NUMBER)));
        } else if (PdfToolbarProperties.TOTAL_PAGE_COUNT == key) {
            TextView pageCount = view.findViewById(R.id.page_count);
            pageCount.setText(String.valueOf(model.get(PdfToolbarProperties.TOTAL_PAGE_COUNT)));
        } else if (PdfToolbarProperties.ZOOM_LEVEL == key) {
            TextView zoomValue = view.findViewById(R.id.zoom_value);
            zoomValue.setText(
                    String.format(
                            java.util.Locale.ENGLISH,
                            "%.0f%%",
                            model.get(PdfToolbarProperties.ZOOM_LEVEL) * 100));
        } else if (PdfToolbarProperties.ON_CLICK_LISTENER == key) {
            View.OnClickListener listener = model.get(PdfToolbarProperties.ON_CLICK_LISTENER);
            view.findViewById(R.id.page_increase_button).setOnClickListener(listener);
            view.findViewById(R.id.page_decrease_button).setOnClickListener(listener);
            view.findViewById(R.id.zoom_increase_button).setOnClickListener(listener);
            view.findViewById(R.id.zoom_decrease_button).setOnClickListener(listener);
        } else if (PdfToolbarProperties.TITLE == key) {
            TextView title = view.findViewById(R.id.pdf_title);
            title.setText(model.get(PdfToolbarProperties.TITLE));
        } else if (PdfToolbarProperties.ZOOM_DECREASE_BUTTON_ENABLED == key) {
            view.findViewById(R.id.zoom_decrease_button)
                    .setEnabled(model.get(PdfToolbarProperties.ZOOM_DECREASE_BUTTON_ENABLED));
        } else if (PdfToolbarProperties.ZOOM_INCREASE_BUTTON_ENABLED == key) {
            view.findViewById(R.id.zoom_increase_button)
                    .setEnabled(model.get(PdfToolbarProperties.ZOOM_INCREASE_BUTTON_ENABLED));
        }
    }
}
