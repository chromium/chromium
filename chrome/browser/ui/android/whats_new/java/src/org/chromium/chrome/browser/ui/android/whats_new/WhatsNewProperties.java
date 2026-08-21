// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.whats_new;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntDefPropertyKey;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@NullMarked
public class WhatsNewProperties {
    /** View states of What's New page. */
    @IntDef({
        ViewState.HIDDEN,
        ViewState.OVERVIEW,
        ViewState.DETAIL,
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface ViewState {
        /* The What's New page is hidden. */
        int HIDDEN = 0;

        /** The What's New page is showing the overview page that list all new feature entries. */
        int OVERVIEW = 1;

        /* The What's New page is showing the detail page of a feature entry. */
        int DETAIL = 2;
    }

    /* PropertyKey indicates the view state of the what's new page. */
    static final WritableIntDefPropertyKey<ViewState> VIEW_STATE =
            new WritableIntDefPropertyKey<>(ViewState.HIDDEN);

    static final PropertyKey[] ALL_KEYS = new PropertyKey[] {VIEW_STATE};
}
