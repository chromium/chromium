// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xsurface.pageinsights;

import android.view.View;

import androidx.annotation.Nullable;

import java.util.Map;

/**
 * Interface to call a rendering service to render a server-provided UI, in the context of the
 * Page Insights feature.
 *
 * Implemented internally.
 */
public interface PageInsightsSurfaceRenderer {
    /**
     * Renders a server-provided UI, and returns a View containing it.
     *
     * @param renderData server-provided UI to be rendered.
     * @param contextValues additional context to be incorporated into the rendered UI.
     */
    @Nullable
    default View render(byte[] renderData, Map<String, Object> contextValues) {
        return null;
    }

    /**
     * Unbinds any server-provided UI from the given View, and cleans up. Should be called whenever
     * the View is no longer shown to the user.
     */
    default void unbindView(View view) {}
}
