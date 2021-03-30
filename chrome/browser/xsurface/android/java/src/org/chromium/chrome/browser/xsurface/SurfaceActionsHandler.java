// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xsurface;

import android.view.View;

/**
 * Interface to provide chromium calling points for an external surface.
 */
public interface SurfaceActionsHandler {
    String KEY = "GeneralActions";

    /**
     * Navigates the current tab to a particular URL.
     * @param url The url for which to navigate.
     * @param actionSourceView The View from which the user tap originated. May be null.
     */
    default void navigateTab(String url, View actionSourceView) {}
    @Deprecated
    default void navigateTab(String url) {
        navigateTab(url, null);
    }

    /**
     * Navigates a new tab to a particular URL.
     * @param url The url for which to navigate.
     * @param actionSourceView The View from which the user tap originated. May be null.
     */
    default void navigateNewTab(String url, View actionSourceView) {}
    @Deprecated
    default void navigateNewTab(String url) {
        navigateNewTab(url, null);
    }

    /**
     * Navigate a new incognito tab to a URL.
     */
    default void navigateIncognitoTab(String url) {}

    /**
     * Get an offline page for a URL.
     */
    default void downloadLink(String url) {}

    /**
     * Open a bottom sheet with the view as contents.
     * @param view The bottom sheet contents view.
     * @param actionSourceView The View from which the user tap originated. May be null.
     */
    default void showBottomSheet(View view, View actionSourceView) {}
    @Deprecated
    default void showBottomSheet(View view) {
        showBottomSheet(view, null);
    }

    /**
     * Dismiss the open bottom sheet (or do nothing if there isn't one).
     */
    default void dismissBottomSheet() {}
}
