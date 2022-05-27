// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.segmentation_platform;

/**
 * Convenient wrapper containing results for on-demand segment selection along with the trigger
 * context required by the UI layer.
 */
public class OnDemandSegmentSelectionResult {
    public final SegmentSelectionResult segmentSelectionResult;
    public final TriggerContext triggerContext;

    /** Constructor */
    public OnDemandSegmentSelectionResult(
            SegmentSelectionResult segmentSelectionResult, TriggerContext triggerContext) {
        this.segmentSelectionResult = segmentSelectionResult;
        this.triggerContext = triggerContext;
    }
}
