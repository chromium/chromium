// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pdf;

import androidx.pdf.view.PdfView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** View binder for the PDF selection menu. */
@NullMarked
class PdfSelectionViewBinder {
    /**
     * Binds the model to the view.
     *
     * @param model The PropertyModel.
     * @param view The PdfView.
     * @param key The PropertyKey that changed.
     */
    static void bind(PropertyModel model, PdfView view, PropertyKey key) {
        if (PdfSelectionProperties.SELECTION_MENU_ITEM_PREPARER == key) {
            PdfSelectionCoordinator.SelectionMenuItemPreparer preparer =
                    model.get(PdfSelectionProperties.SELECTION_MENU_ITEM_PREPARER);
            view.addSelectionMenuItemPreparer(preparer::prepareMenuItems);
        }
    }
}
