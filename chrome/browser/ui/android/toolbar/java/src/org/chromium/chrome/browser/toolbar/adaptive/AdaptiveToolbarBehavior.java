// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.adaptive;

import org.chromium.base.supplier.Supplier;
import org.chromium.components.feature_engagement.Tracker;

import java.util.List;

/** Embedder-specific behavior of Adaptive Toolbar. */
public interface AdaptiveToolbarBehavior {

    /** Returns {@code true} if adaptive toolbar button feature is enabled. */
    default boolean shouldInitialize() {
        return true;
    }

    /**
     * Register embedder-specific toolbar action buttons.
     *
     * @param controller {@link AdaptiveToolbarButtonController} to which the buttons will be added.
     * @param trackerSupplier {@link Tracker} supplier buttons need for instantiation.
     */
    void registerPerSurfaceButtons(
            AdaptiveToolbarButtonController controller, Supplier<Tracker> trackerSupplier);

    /**
     * Filter the segmentation results and pick the one to display on the UI.
     *
     * @param segmentationResults Input prediction results.
     * @return The ID of the picked button.
     */
    @AdaptiveToolbarButtonVariant
    int resultFilter(List<@AdaptiveToolbarButtonVariant Integer> segmentationResults);

    /**
     * Returns {@code true} when it is acceptable to use the raw segmentation result that skips the
     * thresholds to fine-tune model distribution.
     */
    boolean useRawResults();
}
