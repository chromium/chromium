// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.segmentation_platform;

import org.chromium.components.segmentation_platform.proto.SegmentationProto.SegmentId;

/**
 * Java counterpart of native SegmentSelectionResult. Contains the result of segment selection.
 */
public class SegmentSelectionResult {
    /** Whether the backend is ready and has enough signals to compute the segment selection.*/
    public final boolean isReady;

    /**
     * The result of segment selection.
     */
    public final SegmentId selectedSegment;

    /** Constructor */
    public SegmentSelectionResult(boolean isReady, SegmentId selectedSegment) {
        this.isReady = isReady;
        this.selectedSegment = selectedSegment;
    }
}
