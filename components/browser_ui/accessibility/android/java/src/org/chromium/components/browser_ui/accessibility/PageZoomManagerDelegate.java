// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.accessibility;

import org.chromium.build.annotations.NullMarked;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.content_public.browser.WebContents;

/** Delegate interface for |PageZoomManager|. */
@NullMarked
public interface PageZoomManagerDelegate {

    /**
     * @return the WebContents that should be used for the zoom manager.
     */
    WebContents getWebContents();

    /**
     * @return the BrowserContextHandle that should be used for the zoom manager.
     */
    BrowserContextHandle getBrowserContextHandle();

    /**
     * Adds a zoom events observer to the HostZoomListener for the current Profile. This is used to
     * listen for zoom level changes from the native HostZoomMap.
     *
     * @param observer The zoom events observer to add.
     */
    void addZoomEventsObserver(ZoomEventsObserver observer);

    /**
     * Removes a zoom events observer from the HostZoomListener for the current Profile. This is
     * used to stop listening for zoom level changes from the native HostZoomMap.
     *
     * @param observer The zoom events observer to remove.
     */
    void removeZoomEventsObserver(ZoomEventsObserver observer);

    /** Fullscreen the current tab. */
    void enterImmersiveMode();

    boolean isCurrentTabNull();
}
