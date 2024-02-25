// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.segmentation_platform;

import org.chromium.components.segmentation_platform.proto.SegmentationProto.SegmentId;

/** Java counterpart of native SegmentSelectionResult. Contains the result of segment selection. */
public class SegmentSelectionResult {
    /** Whether the backend is ready and has enough signals to compute the segment selection.*/
    public final boolean isReady;

    /** The result of segment selection. */
    public final SegmentId selectedSegment;

    /**
     * The discrete score computed based on the `segment` model execution.
     *
     * If a discrete mapping is not provided, the value will be equal to the model score. Otherwise
     * the value will be the mapped score based on the mapping. May be null if selection was made in
     * versions older than M107.
     */
    public final Float rank;

    /** Constructor */
    public SegmentSelectionResult(boolean isReady, SegmentId selectedSegment, Float rank) {
        this.isReady = isReady;
        this.selectedSegment = selectedSegment;
        this.rank = rank;
    }

    /** Constructor, only used in tests */
    public SegmentSelectionResult(boolean isReady, SegmentId selectedSegment) {
        this(isReady, selectedSegment, null);
    }
}
