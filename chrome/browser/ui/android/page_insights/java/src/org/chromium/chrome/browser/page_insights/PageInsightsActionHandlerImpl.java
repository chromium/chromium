// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_insights;

import org.chromium.chrome.browser.xsurface.pageinsights.PageInsightsActionsHandler;

import java.util.HashMap;

/** Implementation of {@link PageInsightsActionsHandler}. */
class PageInsightsActionHandlerImpl implements PageInsightsActionsHandler {
    // TODO(b/286003870): Implement all methods.

    /**
     * Creates and returns a map containing an instance of this {@link PageInsightsActionsHandler}
     * implementation.
     */
    static HashMap<String, Object> createContextValues() {
        HashMap<String, Object> contextValues = new HashMap<>();
        contextValues.put(PageInsightsActionsHandler.KEY, new PageInsightsActionHandlerImpl());
        return contextValues;
    }
}
