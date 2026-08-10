// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.embedder_support.contextmenu;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.function.Supplier;

/** Factory interface for creating {@link ContextMenuPopulator}s. */
@NullMarked
public interface ContextMenuPopulatorFactory {

    /**
     * Creates a {@link ContextMenuPopulator}.
     *
     * @param context The {@link Context} used to retrieve the strings.
     * @param params The {@link ContextMenuParams} used to build the context menu.
     * @param nativeDelegate The {@link ContextMenuNativeDelegate} for the context menu.
     * @return The new {@link ContextMenuPopulator}.
     */
    ContextMenuPopulator createContextMenuPopulator(
            Context context, ContextMenuParams params, ContextMenuNativeDelegate nativeDelegate);

    /**
     * Sets the {@link ContextMenuItemDelegate} for the context menu.
     *
     * @param itemDelegate The {@link ContextMenuItemDelegate} to set.
     */
    default void setItemDelegate(@Nullable ContextMenuItemDelegate itemDelegate) {}

    /**
     * Whether the factory is enabled. Can be overridden to conditionally disable context menu on
     * certain embedders.
     */
    default boolean isEnabled() {
        return true;
    }

    /** Returns a supplier providing the left side UI width in px. */
    default Supplier<Integer> getLeftSideUiWidthSupplier() {
        return () -> 0;
    }

    void onDestroy();
}
