// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xsurface.pageinsights;

import androidx.annotation.Nullable;

/**
 * Provides renderers for the Page Insights feature to use when rendering a server-provided UI.
 *
 * Implemented internally.
 */
public interface PageInsightsSurfaceScope {
    /**
     * Returns a renderer to use when rendering server-provided UI. Each call will reuse the same
     * dependencies (hence "Scope") but each call will return a new renderer, so that a single
     * surface can support multiple rendered views.
     */
    default @Nullable PageInsightsSurfaceRenderer provideSurfaceRenderer() {
        return null;
    }
}
