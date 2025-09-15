// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.accessibility;

import android.view.View;

import org.chromium.build.annotations.NullMarked;

/** Delegate interface for any class that wants a |PageZoomCoordinator|. */
@NullMarked
public interface PageZoomBarCoordinatorDelegate {
    /**
     * @return the View that should be used to render the zoom control.
     */
    View getZoomControlView();
}
