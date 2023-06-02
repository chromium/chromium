// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xsurface.pageinsights;

/**
 * Interface to handle actions invoked on server-provided UI in the Page Insights feature.
 *
 * Implemented in Chromium.
 */
public interface PageInsightsActionsHandler {
    String KEY = "PageInsightsActionsHandler";

    /**
     * Opens the given URL.
     *
     * @param url URL to open.
     * @param doesRequestSpecifySameSession whether the request specified the URL should be opened
     *         in the same session.
     */
    default void openUrl(String url, boolean doesRequestSpecifySameSession) {}

    /**
     * Brings up sharing sheet to share the given URL.
     *
     * @param url the URL to be shared.
     * @param title the title of the page at the URL.
     */
    default void share(String url, String title) {}

    /**
     * Navigates to a page within the Page Insights feature.
     *
     * @param pageId the ID of the page to navigate to.
     */
    default void navigateToPageInsightsPage(int pageId) {}
}
