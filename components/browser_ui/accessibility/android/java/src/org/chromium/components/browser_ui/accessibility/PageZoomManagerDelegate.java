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
}
