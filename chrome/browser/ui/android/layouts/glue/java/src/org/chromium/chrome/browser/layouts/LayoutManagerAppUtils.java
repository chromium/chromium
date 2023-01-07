// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.layouts;

import org.chromium.ui.base.WindowAndroid;

/** A class used by app code to manage an instance of the {@link LayoutManager}. */
public class LayoutManagerAppUtils {
    /**
     * Attach a shared {@link LayoutManager} to a {@link WindowAndroid}.
     * @param windowAndroid The window to attach the manager to.
     * @param controller The controller to attach.
     */
    public static void attach(WindowAndroid windowAndroid, ManagedLayoutManager controller) {
        LayoutManagerProvider.attach(windowAndroid, controller);
    }

    /**
     * Detach the specified {@link LayoutManager} from any {@link WindowAndroid}s it is associated
     * with.
     * @param controller The manager to remove from any associated windows.
     */
    public static void detach(ManagedLayoutManager controller) {
        LayoutManagerProvider.detach(controller);
    }
}
