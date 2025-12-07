// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.gesture;

import org.chromium.build.annotations.NullMarked;

/** Observer of the system navigation. */
@NullMarked
public interface OnSystemNavigationObserver {
    /**
     * Invoked when a back navigation event is handled by the Android OS itself, rather than by a
     * specific in-app feature (see {@link BackPressHandler}). This typically occurs when the app is
     * about to minimize or close. This callback is intended primary for logging and recording
     * purposes and must not be used for any heavy task.
     *
     * <p>All registered observers will be called and so any two observers should be mutually
     * exclusive.
     */
    void onSystemNavigation();
}
