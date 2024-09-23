// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.embedder_support.contextmenu;

import android.content.Context;

/** Factory interface for creating {@link ContextMenuPopulator}s. */
public interface ContextMenuPopulatorFactory {
    /**
     * Creates a {@ContextMenuPopulator}.
     *
     * @param context The {@link Context} used to retrieve the strings.
     * @param params The {@link ContextMenuParams} used to build the context menu.
     * @param nativeDelegate The {@link ContextMenuNativeDelegate} for the context menu.
     * @return The new {@ContextMenuPopulator}.
     */
    ContextMenuPopulator createContextMenuPopulator(
            Context context, ContextMenuParams params, ContextMenuNativeDelegate nativeDelegate);

    /**
     * Whether the factory is enabled. Can be overridden to conditionally disable context menu on
     * certain embedders.
     */
    default boolean isEnabled() {
        return true;
    }

    void onDestroy();
}
