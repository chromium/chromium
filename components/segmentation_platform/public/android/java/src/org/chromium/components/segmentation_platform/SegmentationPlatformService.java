// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.segmentation_platform;

import org.chromium.base.Callback;

/**
 * SegmentationPlatformService is the core class for segmentation platform.
 * It represents a native SegmentationPlatformService object in Java.
 */
public interface SegmentationPlatformService {
    /**
     * Called to get the segment selection result asynchronously from the backend.
     * @param segmentationKey The key to be used to distinguish between different segmentation
     *         usages.
     * @param callback The callback that contains the result of segmentation.
     */
    void getSelectedSegment(String segmentationKey, Callback<SegmentSelectionResult> callback);

    /**
     * Called to get the segment selection result synchronously from the backend.
     * @param segmentationKey The key to be used to distinguish between different segmentation
     *         usages.
     * @return The result of segment selection
     */
    SegmentSelectionResult getCachedSegmentResult(String segmentationKey);

    /**
     * Called to register a callback to be invoked after a segment selection. Only used for
     * on-demand segment selection.
     * @param segmentationKey The key to be used to distinguish between different clients.
     * @param callback The callback to be invoked after a segment selection is computed.
     * @return A callback ID to be used when unregistering.
     */
    int registerOnDemandSegmentSelectionCallback(
            String segmentationKey, Callback<OnDemandSegmentSelectionResult> callback);

    /**
     * Called to unregister a previously registered callback for segment selection result.
     * @param segmentationKey The key to be used to distinguish between different clients.
     * @param callbackId The associated callback ID obtained when registering.
     */
    void unregisterOnDemandSegmentSelectionCallback(String segmentationKey, int callbackId);
}
