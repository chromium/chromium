// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xsurface;

import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.Nullable;

import java.util.Map;

/**
 * Implemented in Chromium.
 *
 * Interface to provide native views to incorporate in an external surface-controlled
 * RecyclerView.
 *
 * Models after a RecyclerView.Adapter.
 */
public interface ListContentManager {
    /** Returns whether the item at index is a native view or not. */
    default boolean isNativeView(int index) {
        return false;
    }

    /** Gets the bytes needed to render an external view. */
    default byte[] getExternalViewBytes(int index) {
        return new byte[0];
    }

    /** Returns map of values which should go in the context of an external view. */
    @Nullable
    default Map<String, Object> getContextValues(int index) {
        return null;
    }

    /**
     * Returns the inflated native view.
     *
     * View should not be attached to parent. {@link bindNativeView} will
     * be called later to attach more information to the view.
     */
    default View getNativeView(int viewType, ViewGroup parent) {
        return null;
    }

    /**
     * Returns the view type for item at position.
     *
     * The view type is later passed to getNativeView to attach the view.
     * This will only be called when isNativeView(position) is true.
     */
    default int getViewType(int position) {
        // This is an incorrect, but backwards compatible implementation.
        return position;
    }

    /** Binds the data at the specified location. */
    default void bindNativeView(int index, View v) {}

    /** Returns number of items to show. */
    default int getItemCount() {
        return 0;
    }

    /** Returns whether the item at index should span across all columns. */
    default boolean isFullSpan(int index) {
        return false;
    }

    /** Adds an observer to be notified when the list content changes. */
    default void addObserver(ListContentManagerObserver o) {}

    /** Removes the observer so it's no longer notified of content changes. */
    default void removeObserver(ListContentManagerObserver o) {}
}
