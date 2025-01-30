// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.scrim;

import android.content.Context;
import android.view.ViewGroup;

import org.chromium.build.annotations.NullMarked;

/**
 * Replacement and wrapper of {@link ScrimCoordinator}, supporting the creation, display, and
 * observation of multiple simultaneous scrims. Implementation still in progress, see
 * https://crbug.com/371034867.
 */
@NullMarked
public class ScrimManager extends ScrimCoordinator {
    /**
     * @param context An Android {@link Context} for creating the view.
     * @param parent The {@link ViewGroup} the scrim should exist in.
     */
    public ScrimManager(Context context, ViewGroup parent) {
        super(context, parent);
    }
}
