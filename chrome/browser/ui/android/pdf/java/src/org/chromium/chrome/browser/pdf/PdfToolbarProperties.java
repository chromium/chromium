// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pdf;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** The properties required to build the PDF toolbar. */
@NullMarked
class PdfToolbarProperties {
    /** The current page number. */
    static final WritableIntPropertyKey CURRENT_PAGE_NUMBER = new WritableIntPropertyKey();

    /** The title of the PDF document. */
    static final WritableObjectPropertyKey<String> TITLE = new WritableObjectPropertyKey<>();

    /** The total number of pages in the document. */
    static final WritableIntPropertyKey TOTAL_PAGE_COUNT = new WritableIntPropertyKey();

    /** The zoom value text. */
    static final WritableObjectPropertyKey<String> ZOOM_VALUE = new WritableObjectPropertyKey<>();

    /** Whether the zoom decrease button is enabled. */
    static final WritableBooleanPropertyKey ZOOM_DECREASE_BUTTON_ENABLED =
            new WritableBooleanPropertyKey();

    /** Whether the zoom increase button is enabled. */
    static final WritableBooleanPropertyKey ZOOM_INCREASE_BUTTON_ENABLED =
            new WritableBooleanPropertyKey();

    /** The callback for toolbar actions. */
    static final WritableObjectPropertyKey<View.OnClickListener> ON_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();

    static final PropertyKey[] ALL_KEYS = {
        CURRENT_PAGE_NUMBER, TITLE, TOTAL_PAGE_COUNT, ZOOM_VALUE, ON_CLICK_LISTENER,
        ZOOM_DECREASE_BUTTON_ENABLED, ZOOM_INCREASE_BUTTON_ENABLED
    };
}
