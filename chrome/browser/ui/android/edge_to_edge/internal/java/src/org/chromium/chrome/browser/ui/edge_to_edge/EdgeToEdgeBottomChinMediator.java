// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.ui.edge_to_edge;

import android.graphics.Color;

import org.chromium.chrome.browser.browser_controls.BottomControlsStacker;
import org.chromium.ui.modelutil.PropertyModel;

class EdgeToEdgeBottomChinMediator {
    /** The model for the bottom controls component that holds all of its view state. */
    private final PropertyModel mModel;

    private final BottomControlsStacker mBottomControlsStacker;

    /**
     * Build a new mediator for the bottom chin component.
     *
     * @param model The {@link EdgeToEdgeBottomChinProperties} that holds all the view state for the
     *     bottom chin component.
     * @param bottomControlsStacker The {@link BottomControlsStacker} for observing and changing
     *     browser controls heights.
     */
    EdgeToEdgeBottomChinMediator(PropertyModel model, BottomControlsStacker bottomControlsStacker) {
        mModel = model;
        mBottomControlsStacker = bottomControlsStacker;

        mModel.set(EdgeToEdgeBottomChinProperties.Y_OFFSET, 0);
        mModel.set(EdgeToEdgeBottomChinProperties.HEIGHT, 100);
        mModel.set(EdgeToEdgeBottomChinProperties.IS_VISIBLE, true);
        mModel.set(EdgeToEdgeBottomChinProperties.COLOR, Color.RED);
    }

    void destroy() {}
}
