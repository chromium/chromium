// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.native_page;

import android.content.Context;
import android.graphics.Rect;
import android.view.View;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.edge_to_edge.EdgeToEdgePadAdjuster;

/** This interface represents a view that is capable of hosting a NativePage. */
@NullMarked
public interface NativePageHost {
    /**
     * @return A context to use for inflating views and obtaining resources.
     */
    Context getContext();

    /**
     * Load a non-native URL in an active tab. This should be used to either navigate away from the
     * current native page or load external content in a content area (i.e. a tab or web contents).
     *
     * @param urlParams The params describing the URL to be loaded.
     * @param incognito Whether the URL should be loaded in incognito mode.
     */
    void loadUrl(LoadUrlParams urlParams, boolean incognito);

    /**
     * Load a non-native URL in a new tab. This should be used to open hyperlink in a new tab.
     *
     * @param urlParams The params describing the URL to be loaded.
     */
    void openNewTab(LoadUrlParams urlParams);

    /**
     * If the host is a tab, get the ID of its parent.
     *
     * @return The ID of the parent tab or INVALID_TAB_ID.
     */
    int getParentId();

    /** @return whether the hosted native page is currently visible. */
    boolean isVisible();

    /**
     * Creates a default margin adapter. Once created, the NativePage is responsible for calling
     * destroy() to clean-up the adapter once it is no longer needed.
     */
    Destroyable createDefaultMarginAdapter(ObservableSupplierImpl<Rect> supplierImpl);

    /**
     * @return A {@link EdgeToEdgePadAdjuster} to update the edge-to-edge pad.
     */
    EdgeToEdgePadAdjuster createEdgeToEdgePadAdjuster(View view);
}
