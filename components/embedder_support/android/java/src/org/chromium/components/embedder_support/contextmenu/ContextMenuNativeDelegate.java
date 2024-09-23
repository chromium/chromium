// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.embedder_support.contextmenu;

import android.graphics.Bitmap;
import android.net.Uri;

import org.chromium.base.Callback;
import org.chromium.content_public.browser.RenderFrameHost;

/** Interface to handle context menu actions in native. */
public interface ContextMenuNativeDelegate {
    /** Called when this {@link ContextMenuNativeDelegate} is being destroyed. */
    void destroy();

    /**
     * Retrieves the image bitmap from the current {@link RenderFrameHost} that the context menu was
     * triggered on to use in the context menu.
     *
     * @param maxWidthPx The maximum width for the retrieved bitmap in pixels.
     * @param maxHeightPx The maximum height for the retrieved bitmap in pixels.
     * @param callback The callback to be called with the retrieved bitmap.
     */
    void retrieveImageForContextMenu(int maxWidthPx, int maxHeightPx, Callback<Bitmap> callback);

    /**
     * Retrieves the image from the current {@link RenderFrameHost} that the context menu was
     * triggered on, to be shared.
     *
     * @param imageFormat The image format that will be requested.
     * @param callback The callback to be called with the retrieved image's {@link Uri}.
     */
    void retrieveImageForShare(@ContextMenuImageFormat int imageFormat, Callback<Uri> callback);

    /**
     * Starts a download based on the params.
     *
     * @param isLink Whether the download target is a link.
     */
    void startDownload(boolean isLink);

    /** Does a reverse image search for the current image that the context menu was triggered on. */
    void searchForImage();

    /**
     * Get the current {@link RenderFrameHost} that the context menu was triggered on, to be shared.
     *
     * @return {@link RenderFrameHost}.
     */
    RenderFrameHost getRenderFrameHost();
}
