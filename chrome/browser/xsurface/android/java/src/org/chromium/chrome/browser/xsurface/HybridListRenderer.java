// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xsurface;

import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.Nullable;

/**
 * Implemented internally.
 *
 * A renderer that can handle mixing externally-provided views with native Android views
 * in a RecyclerView.
 */
public interface HybridListRenderer {
    /**
     * Binds a contentmanager with this renderer.
     * @return a View that the HybridListRenderer is managing, which can then be
     * attached to other view
     */
    default @Nullable View bind(ListContentManager manager) {
        return null;
    }

    /**
     * Binds a contentmanager with this renderer.
     *
     * @param manager the ListContentManager responsible for populating views
     * @param viewport the ViewGroup containing the content. Views within the bounds of this
     *     ViewGroup will be considered for view actions. If null, the returned View will be used as
     *     the viewport.
     * @param shouldUseStaggeredLayout whether to use Staggered layout for list. Column count should
     *     be set via ListLayoutHelper#setSpanCount()
     * @return a View that the HybridListRenderer is managing, which can then be attached to other
     *     view.
     */
    @Deprecated
    default @Nullable View bind(
            ListContentManager manager,
            @Nullable ViewGroup viewport,
            boolean shouldUseStaggeredLayout) {
        return bind(manager);
    }

    /**
     * Binds a contentmanager with this renderer.
     *
     * @param manager the ListContentManager responsible for populating views
     * @param viewport the ViewGroup containing the content. Views within the bounds of this
     *     ViewGroup will be considered for view actions. If null, the returned View will be used as
     *     the viewport.
     * @param gutterPaddingPerColumnPx the padding in the vertical gutter between the card columns,
     *     per column in pixels when the staggered layout is used for list. Column count should be
     *     set via ListLayoutHelper#setSpanCount(). If the staggered layout is not used, pass -1.
     * @return a View that the HybridListRenderer is managing, which can then be attached to other
     *     view.
     */
    default @Nullable View bind(
            ListContentManager manager,
            @Nullable ViewGroup viewport,
            int gutterPaddingPerColumnPx) {
        return bind(manager);
    }

    /**
     * Notify the HybridListRender when the externally provided view surface (embedded in
     * bind/update) is activated. This should include:
     *
     * <p>- the user opening a new tab containing the (opened) surface. - the user switching to a
     * tab containing the (opened) surface. - the user reactivating the previously deactivated
     * surface.
     */
    default void onSurfaceOpened() {}

    /**
     * Notify the HybridListRender when the externally provided view surface (embedded in
     * bind/update) is deactivated. This should include:
     *
     *   - the user switching to another app.
     *   - the user browsing away to other content.
     *   - the user deactivates the surface.
     *   - the user switching to another tab.
     */
    default void onSurfaceClosed() {}

    /**
     * Unbinds a previously attached recyclerview and contentmanager.
     *
     * Does nothing if nothing was previously bound.
     */
    default void unbind() {}

    /** Updates the renderer with templates and initializing data. */
    default void update(byte[] data) {}

    /** Called when a pull to refresh is initiated by the user. */
    @Deprecated
    default void onPullToRefreshStarted() {}

    /** Called when a manual refresh is initiated by the user. */
    default void onManualRefreshStarted() {}

    /** Returns helper to manager the list layout. @{@link ListLayoutHelper} instance. */
    default ListLayoutHelper getListLayoutHelper() {
        return null;
    }
}
