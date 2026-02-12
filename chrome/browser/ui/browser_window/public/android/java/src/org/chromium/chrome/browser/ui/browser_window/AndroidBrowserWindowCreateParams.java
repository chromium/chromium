// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.browser_window;

import android.graphics.Rect;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.mojom.WindowShowState;

/** Interface defining parameters for creating a browser window on Android. */
@NullMarked
interface AndroidBrowserWindowCreateParams {
    /** Returns the browser window type. */
    @BrowserWindowType
    int getWindowType();

    /** Returns the profile associated with this window. */
    Profile getProfile();

    /** Returns the initial bounds of the window. */
    Rect getInitialBounds();

    /** Returns the initial show state of the window. */
    @WindowShowState.EnumType
    int getInitialShowState();
}
