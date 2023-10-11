// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xsurface.pageinsights;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Events on the Page Insights surface which should result in logging. */
@IntDef({PageInsightsEvent.BOTTOM_SHEET_PEEKING, PageInsightsEvent.BOTTOM_SHEET_EXPANDED,
        PageInsightsEvent.CHILD_PAGE_BACK_BUTTON_VISIBLE,
        PageInsightsEvent.CHILD_PAGE_BACK_BUTTON_TAPPED,
        PageInsightsEvent.DISMISSED_FROM_PEEKING_STATE})
@Retention(RetentionPolicy.SOURCE)
public @interface PageInsightsEvent {
    /** The Page Insights bottom sheet opens automatically in its peeking state. */
    int BOTTOM_SHEET_PEEKING = 0;

    /**
     * The Page Insights bottom sheet enters its expanded state, either by the feature being
     * launched by the user, or by the user dragging up from peek state.
     */
    int BOTTOM_SHEET_EXPANDED = 1;

    /** The child page back button is visible. */
    int CHILD_PAGE_BACK_BUTTON_VISIBLE = 2;

    /** The child page back button is tapped. */
    int CHILD_PAGE_BACK_BUTTON_TAPPED = 3;

    /** The Page Insights bottom sheet is dismissed from its peeking state. */
    int DISMISSED_FROM_PEEKING_STATE = 4;
}
