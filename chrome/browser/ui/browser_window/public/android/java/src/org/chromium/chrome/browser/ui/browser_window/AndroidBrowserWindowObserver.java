// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import org.chromium.build.annotations.NullMarked;

/** Observer interface for {@code AndroidBrowserWindow} lifecycle. */
@NullMarked
public interface AndroidBrowserWindowObserver {
    /**
     * Called when an {@code AndroidBrowserWindow} is added.
     *
     * @param androidBrowserWindowPtr The pointer to the native {@code AndroidBrowserWindow}.
     */
    void onBrowserWindowAdded(long androidBrowserWindowPtr);

    /**
     * Called when an {@code AndroidBrowserWindow} is removed.
     *
     * @param androidBrowserWindowPtr The pointer to the native {@code AndroidBrowserWindow}.
     */
    void onBrowserWindowRemoved(long androidBrowserWindowPtr);
}
