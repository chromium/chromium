// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pdf;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableFloatPropertyKey;
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

    /** Whether the PDF viewer is in two pages per row mode. */
    static final WritableBooleanPropertyKey TWO_PAGES_PER_ROW_ACTIVE =
            new WritableBooleanPropertyKey();

    /** The zoom level. */
    static final WritableFloatPropertyKey ZOOM_LEVEL = new WritableFloatPropertyKey();

    /** Whether the zoom decrease button is enabled. */
    static final WritableBooleanPropertyKey ZOOM_DECREASE_BUTTON_ENABLED =
            new WritableBooleanPropertyKey();

    /** Whether the zoom increase button is enabled. */
    static final WritableBooleanPropertyKey ZOOM_INCREASE_BUTTON_ENABLED =
            new WritableBooleanPropertyKey();

    /** Whether to show the fit to page height icon. */
    static final WritableBooleanPropertyKey SHOW_FIT_TO_HEIGHT_ICON =
            new WritableBooleanPropertyKey();

    /** The callback for toolbar actions. */
    static final WritableObjectPropertyKey<View.OnClickListener> ON_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();

    /** The callback for page number submission. */
    static final WritableObjectPropertyKey<org.chromium.base.Callback<Integer>>
            PAGE_NUMBER_EDIT_LISTENER = new WritableObjectPropertyKey<>();

    /** Whether the download button is visible. */
    static final WritableBooleanPropertyKey DOWNLOAD_BUTTON_VISIBLE =
            new WritableBooleanPropertyKey();

    /** Whether the rotate button is visible. */
    static final WritableBooleanPropertyKey ROTATE_BUTTON_VISIBLE =
            new WritableBooleanPropertyKey();

    /** Whether the fit to page button is visible. */
    static final WritableBooleanPropertyKey FIT_TO_PAGE_BUTTON_VISIBLE =
            new WritableBooleanPropertyKey();

    /** Whether the zoom controls are visible. */
    static final WritableBooleanPropertyKey ZOOM_CONTROLS_VISIBLE =
            new WritableBooleanPropertyKey();

    /** Whether the page navigation and edit controls are visible. */
    static final WritableBooleanPropertyKey PAGE_NAV_AND_EDIT_VISIBLE =
            new WritableBooleanPropertyKey();

    static final PropertyKey[] ALL_KEYS = {
        CURRENT_PAGE_NUMBER,
        TITLE,
        TOTAL_PAGE_COUNT,
        ZOOM_LEVEL,
        ON_CLICK_LISTENER,
        TWO_PAGES_PER_ROW_ACTIVE,
        ZOOM_DECREASE_BUTTON_ENABLED,
        ZOOM_INCREASE_BUTTON_ENABLED,
        PAGE_NUMBER_EDIT_LISTENER,
        SHOW_FIT_TO_HEIGHT_ICON,
        DOWNLOAD_BUTTON_VISIBLE,
        ROTATE_BUTTON_VISIBLE,
        FIT_TO_PAGE_BUTTON_VISIBLE,
        ZOOM_CONTROLS_VISIBLE,
        PAGE_NAV_AND_EDIT_VISIBLE
    };
}
