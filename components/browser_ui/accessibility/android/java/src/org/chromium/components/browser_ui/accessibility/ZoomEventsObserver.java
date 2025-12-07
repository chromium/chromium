// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.accessibility;

import org.chromium.build.annotations.NullMarked;

/** Interface for observing zoom level changes. */
@FunctionalInterface
@NullMarked
public interface ZoomEventsObserver {
    /**
     * Called when the zoom level for a host has changed.
     *
     * @param host The host for which the zoom level changed.
     * @param zoomLevel The new zoom level.
     */
    void onZoomLevelChanged(String host, double zoomLevel);
}
