// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.accessibility;

import android.view.View;

import org.chromium.content_public.browser.BrowserContextHandle;

/** Delegate interface for any class that wants a |PageZoomCoordinator|. */
public interface PageZoomCoordinatorDelegate {
    /** @return the View that should be used to render the zoom control. */
    View getZoomControlView();

    /** @return the BrowserContextHandle that should be used for the zoom control. */
    BrowserContextHandle getBrowserContextHandle();
}
