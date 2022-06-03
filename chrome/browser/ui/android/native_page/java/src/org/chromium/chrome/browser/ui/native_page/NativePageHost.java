// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.native_page;

import android.content.Context;
import android.graphics.Rect;

import org.chromium.base.supplier.DestroyableObservableSupplier;
import org.chromium.content_public.browser.LoadUrlParams;

/**
 * This interface represents a view that is capable of hosting a NativePage.
 */
public interface NativePageHost {
    /**
     * @return A context to use for inflating views and obtaining resources.
     */
    Context getContext();

    /**
     * Load a non-native URL in an active tab. This should be used to either navigate away from
     * the current native page or load external content in a content area (i.e. a tab or web
     * contents).
     * @param urlParams The params describing the URL to be loaded.
     * @param incognito Whether the URL should be loaded in incognito mode.
     */
    void loadUrl(LoadUrlParams urlParams, boolean incognito);

    /**
     * If the host is a tab, get the ID of its parent.
     * @return The ID of the parent tab or INVALID_TAB_ID.
     */
    int getParentId();

    /** @return whether the hosted native page is currently visible. */
    boolean isVisible();

    /**
     * Creates a default margin supplier. Once created, the NativePage is responsible for calling
     * {@link DestroyableObservableSupplier#destroy()} to clean-up the supplier once it is no longer
     * needed.
     * @return A {@link DestroyableObservableSupplier} to use for setting margins.
     */
    DestroyableObservableSupplier<Rect> createDefaultMarginSupplier();
}
