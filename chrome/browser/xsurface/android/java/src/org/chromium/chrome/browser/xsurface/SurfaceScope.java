// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xsurface;

import androidx.annotation.Nullable;

/**
 * Implemented internally.
 *
 * Provides multiple types of renderers to surfaces that want to render an
 * external surface. Each renderer will reuse the same dependencies (hence
 * "Scope") but each call to provideFoo will return a new renderer, so that a
 * single surface can support multiple rendered views.
 */
public interface SurfaceScope {
    default @Nullable HybridListRenderer provideListRenderer() {
        return null;
    }

    default @Nullable SurfaceRenderer provideSurfaceRenderer() {
        return null;
    }

    default void replaceDataStoreEntry(String key, byte[] data) {}

    default void removeDataStoreEntry(String key) {}
}
