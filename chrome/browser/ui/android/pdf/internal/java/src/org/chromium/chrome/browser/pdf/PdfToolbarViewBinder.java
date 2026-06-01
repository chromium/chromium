// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.pdf;

import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.widget.EditText;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Locale;

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
            if (!currentPage.isFocused()) {
                currentPage.setText(
                        String.valueOf(model.get(PdfToolbarProperties.CURRENT_PAGE_NUMBER)));
            }
        } else if (PdfToolbarProperties.TOTAL_PAGE_COUNT == key) {
            TextView pageCount = view.findViewById(R.id.page_count);
            pageCount.setText(String.valueOf(model.get(PdfToolbarProperties.TOTAL_PAGE_COUNT)));
        } else if (PdfToolbarProperties.ZOOM_LEVEL == key) {
            TextView zoomValue = view.findViewById(R.id.zoom_value);
            zoomValue.setText(
                    String.format(
                            Locale.ENGLISH,
                            "%.0f%%",
                            model.get(PdfToolbarProperties.ZOOM_LEVEL) * 100));
        } else if (PdfToolbarProperties.ON_CLICK_LISTENER == key) {
            View.OnClickListener listener = model.get(PdfToolbarProperties.ON_CLICK_LISTENER);
            view.findViewById(R.id.zoom_increase_button).setOnClickListener(listener);
            view.findViewById(R.id.zoom_decrease_button).setOnClickListener(listener);
            view.findViewById(R.id.fit_to_page_button).setOnClickListener(listener);
            view.findViewById(R.id.more_menu_button).setOnClickListener(listener);
            view.findViewById(R.id.download_button).setOnClickListener(listener);
            view.findViewById(R.id.rotate_button).setOnClickListener(listener);
        } else if (PdfToolbarProperties.TITLE == key) {
            TextView title = view.findViewById(R.id.pdf_title);
            title.setText(model.get(PdfToolbarProperties.TITLE));
        } else if (PdfToolbarProperties.ZOOM_DECREASE_BUTTON_ENABLED == key) {
            view.findViewById(R.id.zoom_decrease_button)
                    .setEnabled(model.get(PdfToolbarProperties.ZOOM_DECREASE_BUTTON_ENABLED));
        } else if (PdfToolbarProperties.ZOOM_INCREASE_BUTTON_ENABLED == key) {
            view.findViewById(R.id.zoom_increase_button)
                    .setEnabled(model.get(PdfToolbarProperties.ZOOM_INCREASE_BUTTON_ENABLED));
        } else if (PdfToolbarProperties.PAGE_NUMBER_EDIT_LISTENER == key) {
            EditText currentPage = view.findViewById(R.id.current_page);
            Callback<Integer> listener = model.get(PdfToolbarProperties.PAGE_NUMBER_EDIT_LISTENER);

            currentPage.setOnEditorActionListener(
                    (v, actionId, event) -> {
                        if (actionId != EditorInfo.IME_ACTION_GO
                                && actionId != EditorInfo.IME_ACTION_DONE) {
                            return false;
                        }
                        boolean isSuccess = false;
                        String text = currentPage.getText().toString();
                        if (!text.isEmpty()) {
                            int pageNumber = Integer.parseInt(text);
                            int totalPageCount = model.get(PdfToolbarProperties.TOTAL_PAGE_COUNT);

                            if (pageNumber >= 1 && pageNumber <= totalPageCount) {
                                listener.onResult(pageNumber);
                                isSuccess = true;
                            }
                        }
                        // If the input was invalid, reset the text to the current page
                        if (!isSuccess) {
                            int currentFallback =
                                    model.get(PdfToolbarProperties.CURRENT_PAGE_NUMBER);
                            currentPage.setText(String.valueOf(currentFallback));
                        }
                        currentPage.clearFocus();
                        return true;
                    });

        } else if (PdfToolbarProperties.SHOW_FIT_TO_HEIGHT_ICON == key) {
            ImageView fitToPageButton = view.findViewById(R.id.fit_to_page_button);
            if (model.get(PdfToolbarProperties.SHOW_FIT_TO_HEIGHT_ICON)) {
                fitToPageButton.setImageResource(R.drawable.ic_fit_page_height_24dp);
            } else {
                fitToPageButton.setImageResource(R.drawable.ic_fit_page_width_24dp);
            }
        }
    }
}
